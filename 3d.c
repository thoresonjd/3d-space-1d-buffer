/**
 * @file 3d.c
 * @brief Rotate a third-order tensor represented by a one-dimensional buffer.
 * @author Justin Thoreson
 *
 * A third-order tensor can be thought of as the three-dimensional equivalent
 * of an array or matrix. Here, I represent a third-order tensor with a simple
 * one-dimensional buffer. In this case, the third-order tensor has uniform
 * dimensions (the width, height, and depth are all equal to one another).
 *
 * In the one-dimensional buffer, each index must map to an element. Therefore,
 * I treat the dimensions of the third-order tensor with the width as the most
 * significant dimension, the height as the next significant dimension, and the
 * depth as the least significant dimension. Thus, it follows that whenever the
 * index increments by one, then the width increases by one; whenever the index
 * increments to a number divisible by the width, then the height increases by
 * one; whenever the index increments to a number divisible by the width
 * multiplied by the height, the depth increases by one. The width corresponds
 * to the x-axis, the height corresponds to the y-axis, and the depth
 * corresponds to the z-axis. The positive direction of all axes are in the
 * direction the dimension and index increases.
 *
 *            18 19 20
 *            21 22 23
 *            24 25 26
 *                    /
 *         9 10 11   /
 *        12 13 14  / h
 *        15 16 17 / t
 *                / p
 * h   width     / e
 * e   -----    / d
 * i | 0 1 2   /
 * g | 3 4 5  /
 * h | 6 7 8 /
 * t
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/**
 * @brief The program's minimum allowed dimension for a third-order tensor.
 */
const uint8_t TENSOR3_DIM_MIN = 3;

/**
 * @brief The program's maximum allowed dimension for a third-order tensor.
 */
const uint8_t TENSOR3_DIM_MAX = 50;

/**
 * @brief A third-order tensor represented by a one-dimensional buffer.
 */
typedef struct {
	uint8_t* buffer;
	uint8_t dimension;
	uint8_t section;
	uint16_t section_size;
	uint32_t size;
} tensor3_t;

/**
 * @brief A coordinate representing an element within a third-order tensor.
 */
typedef struct {
	uint8_t x, y, z;
} coordinate_t;

/**
 * @brief Four coordinates within a third-order tensor whose elements must be
 *        rotated 90 degress.
 *
 * Rotating the elements of a matrix (or, rather, a cross section of a
 * third-order tensor) 90 degrees cannot be done completely in-place, as
 * elements will be overwritten. To do in-place rotation, and even just to
 * calculate the corresponding index in which an element will be rotated to,
 * we can group elements of the cross section into quartets: groups of four
 * that can be rotated in tandem. Instead of creating a copy of the entire
 * cross section, only the quartet of elements need to be copied to prevent
 * overwritting the data which is also rotated. Element one takes the position
 * of element two, element two takes the position of element three, element
 * three takes the position of element four, and element four takes the
 * position of element one. This structure tracks the coordinates of the four
 * cooresponding elements.
 */
typedef struct {
	coordinate_t first, second, third, fourth;
} quartet_t;

/**
 * @brief Enumeration of all axes of a third-order tensor.
 *
 * The positive x-axis points to the right.
 * The negative x-axis points to the left.
 * The positive y-axis points downward.
 * The negative y-axis points upward.
 * The positive z-axis points inward (toward the screen).
 * The negative z-axis points outward (toward the viewer).
 *
 *           -y    +z (far)
 *             |  /
 *             | /
 *             |/
 *     -x -----+----- +x
 *            /
 *           / |
 *          /  |
 * (near) -z    +y
 */
typedef enum {
	AXIS_XPOSITIVE,	
	AXIS_XNEGATIVE,
	AXIS_YPOSITIVE,	
	AXIS_YNEGATIVE,
	AXIS_ZPOSITIVE,	
	AXIS_ZNEGATIVE,
} axis_t;

/**
 * @brief Retrieve the current terminal settings.
 * @return The parameters of the current terminal.
 */
static struct termios terminal_get() {
	struct termios terminal;
	tcgetattr(STDIN_FILENO, &terminal);
	return terminal;
}

/**
 * @brief Set the attributes of the current terminal.
 * @param[in] terminal The terminal parameters to set.
 */
static void terminal_set(const struct termios* terminal) {
	tcsetattr(STDIN_FILENO, TCSANOW, terminal);
	fflush(stdout);
}

/**
 * @brief Enable noncanonical mode/disable canonical mode and disable the
 *        echoing for the given terminal attributes.
 * @param[in] terminal The provided terminal settings to modify.
 * @return A copy of the provided terminal settings in noncanonical mode.
 */
static struct termios terminal_noncanon(struct termios terminal) {
	terminal.c_lflag &= ~(ICANON | ECHO);
	return terminal;
}

/**
 * @brief Initialize the terminal configuration.
 * @return The attributes of the terminal prior to initialization
 */
static struct termios terminal_init() {
	struct termios orig_terminal = terminal_get();
	struct termios new_terminal = terminal_noncanon(orig_terminal);
	terminal_set(&new_terminal);
	return orig_terminal;
}

/**
 * @brief Clear the terminal and reset the cursor.
 */
static void terminal_clear() {
	printf("\x1b[2J\x1b[H");
}

/**
 * @brief Parse an unsigned 8-bit integer from a string.
 * @param[in] arg The string to parse for a uint8 value.
 * @param[out] value The parsed uint8 value.
 * @return true if a uint8 value was parsed successfully, false otherwise
 */
static bool uint8_parse(const char* const arg, uint8_t* const value) {
	if (!arg || !value)
		return false;
	int64_t temp_value;
	if (!sscanf(arg, "%ld", &temp_value))
		return false;
	if (temp_value < 0 || temp_value > UINT8_MAX)
		return false;
	*value = (uint8_t)temp_value;
	return true;
}

/**
 * @brief Initialize a third-order tensor from string arguments.
 * @param[in] argc The number of arguments.
 * @param[in] argv An array of arguments.
 * @param[out] tensor3 The third-order tensor to initialize.
 * @return true if the third-order tensor was initialized, false otherwise
 */
static bool tensor3_init_from_args(
	const int* const argc,
	char** const argv,
	tensor3_t* const tensor3
) {
	if (*argc != 2)
		return false;
	if (!uint8_parse(argv[1], &tensor3->dimension))
		return false;
	if (tensor3->dimension < TENSOR3_DIM_MIN || tensor3->dimension > TENSOR3_DIM_MAX)
		return false;
	tensor3->section = 0;
	tensor3->section_size = tensor3->dimension * tensor3->dimension;
	tensor3->size = tensor3->section_size * tensor3->dimension;
	tensor3->buffer = (uint8_t*)malloc(tensor3->size);
	if (!tensor3->buffer)
		return false;
	for (int i = 0; i < tensor3->size; i++)
		tensor3->buffer[i] = (i / tensor3->section_size) % ('Z' - 'A') + 'A';
	return true;
}

/**
 * @brief Calculate the index of a third-order tensor given a coordinate.
 * @param[in] coord A coordinate structure to convert to an index.
 * @param[in] tensor3 The third-order tensor to calculate the index for.
 * @return The calculated index.
 *
 * Formula: index = x + (y * width) + (z * width * height)
 */
static uint32_t tensor3_coord_to_index(
	const coordinate_t* const coord,
	const tensor3_t* const tensor3
) {
	return coord->x + (coord->y * tensor3->dimension) + (coord->z * tensor3->section_size);
}

/**
 * @brief Convert an index to a corresponding coordinate.
 * @param[out] coord The coordinate to calculate.
 * @param[in] idx The index to convert into a coordinate.
 * @param[in] tensor3 The third-order tensor to calculate the coordinate for.
 * @return true if the conversion was successful, false otherwise
 *
 * Every time the index increases by one width, the x value resets.
 * x = index % width
 *
 * When the index increases by one width, the height increases by one. Once the
 * index increases by width times height, the y value resets.
 * y = (index / width) % height
 *
 * As the index increases by width times height, the z value increases by one.
 * z = index / (width * height)
 */
[[maybe_unused]] static bool tensor3_index_to_coord(
	coordinate_t* const coord,
	const uint32_t* const idx,
	const tensor3_t* const tensor3
) {
	if (*idx >= tensor3->size)
		return false;
	coord->x = *idx % tensor3->dimension;
	coord->y = (*idx / tensor3->dimension) % tensor3->dimension;
	coord->z = *idx / tensor3->section_size;
	return true;
}

/**
 * @brief Calculate a quartet of coordinates to rotate in a third-order tensor.
 * @param[out] quartet The quartet of coordinates under calculation.
 * @param[in] tensor3 The third-order tensor to calculate the quartet of.
 * @param[in] section The section of the tensor to calculate the quartet for.
 * @param[in] layer The layer within the section.
 * @param[in] offset The offset within the layer.
 * @param[in] axis The axis to rotate about.
 * @return true if the calculation was successful, false otherwise
 */
static bool tensor3_calculate_quartet(
	tensor3_t* const tensor3,
	quartet_t* const quartet,
	const uint8_t* const section,
	const uint8_t* const layer,
	const uint8_t* const offset,
	const axis_t* const axis
) {
	if (*section >= tensor3->dimension
		|| *layer >= tensor3->section_size / 2
		|| *offset >= tensor3->dimension - 1 - 2 * *layer)
		return false;
	switch (*axis) {
		case AXIS_XPOSITIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = *section,
					.y = *layer,
					.z = tensor3->dimension - 1 - *layer - *offset
				},
				.second = (coordinate_t){
					.x = *section,
					.y = *layer + *offset,
					.z = *layer
				},
				.third = (coordinate_t){
					.x = *section,
					.y = tensor3->dimension - 1 - *layer,
					.z = *layer + *offset
				},
				.fourth = (coordinate_t){
					.x = *section,
					.y = tensor3->dimension - 1 - *layer - *offset,
					.z = tensor3->dimension - 1 - *layer
				}
			};
			break;
		case AXIS_XNEGATIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = tensor3->dimension - 1 - *section,
					.y = *layer,
					.z = *layer + *offset
				},
				.second = (coordinate_t){
					.x = tensor3->dimension - 1 - *section,
					.y = *layer + *offset,
					.z = tensor3->dimension - 1 - *layer
				},
				.third = (coordinate_t){
					.x = tensor3->dimension - 1 - *section,
					.y = tensor3->dimension - 1 - *layer,
					.z = tensor3->dimension - 1 - *layer - *offset
				},
				.fourth = (coordinate_t){
					.x = tensor3->dimension - 1 - *section,
					.y = tensor3->dimension - 1 - *layer - *offset,
					.z = *layer
				}
			};
			break;
		case AXIS_YPOSITIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = *layer + *offset,
					.y = *section,
					.z = tensor3->dimension - 1 - *layer
				},
				.second = (coordinate_t){
					.x = tensor3->dimension - 1 - *layer,
					.y = *section,
					.z = tensor3->dimension - 1 - *layer - *offset
				},
				.third = (coordinate_t){
					.x = tensor3->dimension - 1 - *layer - *offset,
					.y = *section,
					.z = *layer
				},
				.fourth = (coordinate_t){
					.x = *layer,
					.y = *section,
					.z = *layer + *offset
				},
			};
			break;
		case AXIS_YNEGATIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = *layer + *offset,
					.y = tensor3->dimension - 1 - *section,
					.z = *layer
				},
				.second = (coordinate_t){
					.x = tensor3->dimension - 1 - *layer,
					.y = tensor3->dimension - 1 - *section,
					.z = *layer + *offset
				},
				.third = (coordinate_t){
					.x = tensor3->dimension - 1 - *layer - *offset,
					.y = tensor3->dimension - 1 - *section,
					.z = tensor3->dimension - 1 - *layer
				},
				.fourth = (coordinate_t){
					.x = *layer,
					.y = tensor3->dimension - 1 - *section,
					.z = tensor3->dimension - 1 - *layer - *offset
				}
			};
			break;
		case AXIS_ZPOSITIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = *layer + *offset,
					.y = *layer,
					.z = *section
				},
				.second = (coordinate_t){
					.x = tensor3->dimension - 1 - *layer,
					.y = *layer + *offset,
					.z = *section
				},
				.third = (coordinate_t){
					.x = tensor3->dimension - 1 - *layer - *offset,
					.y = tensor3->dimension - 1 - *layer,
					.z = *section
				},
				.fourth = (coordinate_t){
					.x = *layer,
					.y = tensor3->dimension - 1 - *layer - *offset,
					.z = *section
				}
			};
			break;
		case AXIS_ZNEGATIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = tensor3->dimension - 1 - *layer - *offset,
					.y = *layer,
					.z = tensor3->dimension - 1 - *section
				},
				.second = (coordinate_t){
					.x = *layer,
					.y = *layer + *offset,
					.z = tensor3->dimension - 1 - *section
				},
				.third = (coordinate_t){
					.x = *layer + *offset,
					.y = tensor3->dimension - 1 - *layer,
					.z = tensor3->dimension - 1 - *section
				},
				.fourth = (coordinate_t){
					.x = tensor3->dimension - 1 - *layer,
					.y = tensor3->dimension - 1 - *layer - *offset,
					.z = tensor3->dimension - 1 - *section
				},
			};
			break;
		default:
			return false;
	}
	return true;
}

/**
 * @brief Rotate a quartet of coordinates in a layer of a third-order tensor.
 * @param[in,out] tensor3 The third-order tensor being rotated.
 * @param[in] section The section being rotated.
 * @param[in] layer The layer being rotated.
 * @param[in] offset The offset within the layer used to calculate the quartet.
 * @param[in] axis The axis to rotate about.
 * @return true if the rotation was successful, false otherwise
 */
static bool tensor3_rotate_quartet(
	tensor3_t* const tensor3,
	const uint8_t* const section,
	const uint8_t* const layer,
	const uint8_t* const offset,
	const axis_t* const axis
) {
	quartet_t quartet = { 0 };
	if (!tensor3_calculate_quartet(tensor3, &quartet, section, layer, offset, axis))
		return false;
	uint32_t first_idx = tensor3_coord_to_index(&quartet.first, tensor3);
	uint32_t second_idx = tensor3_coord_to_index(&quartet.second, tensor3);
	uint32_t third_idx = tensor3_coord_to_index(&quartet.third, tensor3);
	uint32_t fourth_idx = tensor3_coord_to_index(&quartet.fourth, tensor3);
	char first = tensor3->buffer[first_idx];
	char second = tensor3->buffer[second_idx];
	char third = tensor3->buffer[third_idx];
	char fourth = tensor3->buffer[fourth_idx];
	tensor3->buffer[first_idx] = fourth;
	tensor3->buffer[second_idx] = first;
	tensor3->buffer[third_idx] = second;
	tensor3->buffer[fourth_idx] = third;
	return true;
}

/**
 * @brief Rotate a layer of a cross section of a third-order tensor 90 degrees.
 * @param[in,out] tensor3 The third-order tensor being rotated.
 * @param[in] section The section being rotated.
 * @param[in] layer The layer to rotate.
 * @param[in] axis The axis to rotate about.
 * @return true if the rotation was successful, false otherwise
 */
static bool tensor3_rotate_layer(
	tensor3_t* const tensor3,
	const uint8_t* const section,
	const uint8_t* const layer,
	const axis_t* const axis
) {
	const uint8_t offset_end = tensor3->dimension - 1 - 2 * *layer;
	for (uint8_t offset = 0; offset < offset_end; offset++)
		if (!tensor3_rotate_quartet(tensor3, section, layer, &offset, axis))
			return false;
	return true;
}

/**
 * @brief Rotate a cross section of a third-order tensor 90 degrees.
 * @param[in,out] tensor3 The third-order tensor being rotated.
 * @param[in] section The section to rotate.
 * @param[in] axis The axis to rotate about.
 * @return true if the rotation was successful, false otherwise
 */
static bool tensor3_rotate_section(
	tensor3_t* const tensor3,
	const uint8_t* const section,
	const axis_t* const axis
) {
	for (uint8_t layer = 0; layer < tensor3->dimension / 2; layer++)
		if (!tensor3_rotate_layer(tensor3, section, &layer, axis))
			return false;
	return true;
}

/**
 * @brief Rotate a third-order tensor 90 degrees.
 * @param[in,out] tensor3 The third-order tensor to rotate.
 * @param[in] axis The axis to rotate about.
 * @return true if the rotation was successful, false otherwise
 */
static bool tensor3_rotate(tensor3_t* const tensor3, const axis_t axis) {
	for (uint8_t section = 0; section < tensor3->dimension; section++) 
		if (!tensor3_rotate_section(tensor3, &section, &axis))
			return false;
	return true;
}

/**
 * @brief Process keyboard input.
 * @param[in,out] tensor3 The third-order tensor to rotate.
 * @return true if the input was processed successfully, false otherwise.
 */
static bool tensor3_process_input(tensor3_t* const tensor3) {
	char c;
	if (read(STDIN_FILENO, &c, 1) <= 0)
		return false;
	switch (c) {
		case 'x':
			// quit; currently, returning false will terminate the program
			return false;
		case 'w':
			tensor3_rotate(tensor3, AXIS_XNEGATIVE);
			break;
		case 's':
			tensor3_rotate(tensor3, AXIS_XPOSITIVE);
			break;
		case 'a':
			tensor3_rotate(tensor3, AXIS_YPOSITIVE);
			break;
		case 'd':
			tensor3_rotate(tensor3, AXIS_YNEGATIVE);
			break;
		case 'q':
			tensor3_rotate(tensor3, AXIS_ZNEGATIVE);
			break;
		case 'e':
			tensor3_rotate(tensor3, AXIS_ZPOSITIVE);
			break;
		// UP and DOWN arrow keys are used to move sections
		case '\x1b': { // ANSI escape code
			getchar(); // skip [
			char value = getchar();
			if (value == 'A' && tensor3->section < tensor3->dimension - 1)
				tensor3->section++;
			else if (value == 'B' && tensor3->section > 0)
				tensor3->section--;
			break;
		}
	}
	return true;
}

/**
 * @brief Print (a section of) the third-order tensor to the terminal.
 * @param[in] tensor3 The third-order tensor to render.
 */
static void tensor3_render(const tensor3_t* const tensor3) {
	const coordinate_t coord = {0, 0, tensor3->section};
	const uint16_t index_start = tensor3_coord_to_index(&coord, tensor3);
	const uint16_t index_end = (tensor3->section + 1) * tensor3->section_size;
	for (uint16_t i = index_start; i < index_end && i < tensor3->size; i++) {
		putchar(tensor3->buffer[i]);
		if (i % tensor3->dimension == tensor3->dimension - 1)
			putchar('\n');
	}
}

int main(int argc, char** argv) {
	tensor3_t tensor3;
	if (!tensor3_init_from_args(&argc, argv, &tensor3))
		return 1;
	struct termios orig_terminal = terminal_init();
	do {
		terminal_clear();
		tensor3_render(&tensor3);
	} while (tensor3_process_input(&tensor3));
	terminal_set(&orig_terminal);
	return 0;
}

