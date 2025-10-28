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
#include <string.h>

static const uint8_t DIMENSION = 4;
static const uint16_t SECTION_SIZE = DIMENSION * DIMENSION;
static const uint32_t TENSOR3_SIZE = SECTION_SIZE * DIMENSION;

/**
 * @brief A third-order tensor represented by a one-dimensional buffer.
 */
typedef uint8_t tensor3_t;

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
 * @brief Calculate the index of a third-order tensor given a coordinate.
 * @param[in] coordinate_t A coordinate structure to convert to an index.
 * @return The calculated index.
 *
 * Formula: index = x + (y * width) + (z * width * height)
 */
static uint32_t coord_to_index(const coordinate_t* const coord) {
	return coord->x + (coord->y * DIMENSION) + (coord->z * SECTION_SIZE);
}

/**
 * @brief Convert an index to a corresponding coordinate.
 * @param[out] coord The coordinate to calculate.
 * @param[in] idx The index to convert into a coordinate.
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
[[maybe_unused]] static bool index_to_coord(
	coordinate_t* const coord,
	const uint32_t* const idx
) {
	if (*idx >= TENSOR3_SIZE)
		return false;
	coord->x = *idx % DIMENSION;
	coord->y = (*idx / DIMENSION) % DIMENSION;
	coord->z = *idx / SECTION_SIZE;
	return true;
}

/**
 * @brief Calculate a quartet of coordinates to rotate in a third-order tensor.
 * @param[out] quartet The quartet of coordinates under calculation.
 * @param[in] section The section of the tensor.
 * @param[in] layer The layer within the section.
 * @param[in] offset The offset within the layer.
 * @param[in] axis The axis to rotate about.
 * @return true if the calculation was successful, false otherwise
 */
static bool calculate_quartet(
	quartet_t* const quartet,
	const uint8_t section,
	const uint8_t layer,
	const uint8_t offset,
	const axis_t axis
) {
	if (section >= DIMENSION
		|| layer >= SECTION_SIZE / 2
		|| offset >= DIMENSION - 1 - 2 * layer)
		return false;
	switch (axis) {
		case AXIS_XPOSITIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = section,
					.y = layer,
					.z = DIMENSION - 1 - layer - offset
				},
				.second = (coordinate_t){
					.x = section,
					.y = layer + offset,
					.z = layer
				},
				.third = (coordinate_t){
					.x = section,
					.y = DIMENSION - 1 - layer,
					.z = layer + offset
				},
				.fourth = (coordinate_t){
					.x = section,
					.y = DIMENSION - 1 - layer - offset,
					.z = DIMENSION - 1 - layer
				}
			};
			break;
		case AXIS_XNEGATIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = DIMENSION - 1 - section,
					.y = layer,
					.z = layer + offset
				},
				.second = (coordinate_t){
					.x = DIMENSION - 1 - section,
					.y = layer + offset,
					.z = DIMENSION - 1 - layer
				},
				.third = (coordinate_t){
					.x = DIMENSION - 1 - section,
					.y = DIMENSION - 1 - layer,
					.z = DIMENSION - 1 - layer - offset
				},
				.fourth = (coordinate_t){
					.x = DIMENSION - 1 - section,
					.y = DIMENSION - 1 - layer - offset,
					.z = layer
				}
			};
			break;
		case AXIS_YPOSITIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = layer + offset,
					.y = section,
					.z = DIMENSION - 1 - layer
				},
				.second = (coordinate_t){
					.x = DIMENSION - 1 - layer,
					.y = section,
					.z = DIMENSION - 1 - layer - offset
				},
				.third = (coordinate_t){
					.x = DIMENSION - 1 - layer - offset,
					.y = section,
					.z = layer
				},
				.fourth = (coordinate_t){
					.x = layer,
					.y = section,
					.z = layer + offset
				},
			};
			break;
		case AXIS_YNEGATIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = layer + offset,
					.y = DIMENSION - 1 - section,
					.z = layer
				},
				.second = (coordinate_t){
					.x = DIMENSION - 1 - layer,
					.y = DIMENSION - 1 - section,
					.z = layer + offset
				},
				.third = (coordinate_t){
					.x = DIMENSION - 1 - layer - offset,
					.y = DIMENSION - 1 - section,
					.z = DIMENSION - 1 - layer
				},
				.fourth = (coordinate_t){
					.x = layer,
					.y = DIMENSION - 1 - section,
					.z = DIMENSION - 1 - layer - offset
				}
			};
			break;
		case AXIS_ZPOSITIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = layer + offset,
					.y = layer,
					.z = section
				},
				.second = (coordinate_t){
					.x = DIMENSION - 1 - layer,
					.y = layer + offset,
					.z = section
				},
				.third = (coordinate_t){
					.x = DIMENSION - 1 - layer - offset,
					.y = DIMENSION - 1 - layer,
					.z = section
				},
				.fourth = (coordinate_t){
					.x = layer,
					.y = DIMENSION - 1 - layer - offset,
					.z = section
				}
			};
			break;
		case AXIS_ZNEGATIVE:
			*quartet = (quartet_t){
				.first = (coordinate_t){
					.x = DIMENSION - 1 - layer - offset,
					.y = layer,
					.z = DIMENSION - 1 - section
				},
				.second = (coordinate_t){
					.x = layer,
					.y = layer + offset,
					.z = DIMENSION - 1 - section
				},
				.third = (coordinate_t){
					.x = layer + offset,
					.y = DIMENSION - 1 - layer,
					.z = DIMENSION - 1 - section
				},
				.fourth = (coordinate_t){
					.x = DIMENSION - 1 - layer,
					.y = DIMENSION - 1 - layer - offset,
					.z = DIMENSION - 1 - section
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
 * @param[in,out] buffer The third-order tensor being rotated.
 * @param[in] section The section being rotated.
 * @param[in] layer The layer being rotated.
 * @param[in] offset The offset within the layer used to calculate the quartet.
 * @param[in] axis The axis to rotate about.
 * @return true if the rotation was successful, false otherwise
 */
static bool rotate_quartet(
	tensor3_t tensor3[TENSOR3_SIZE],
	const uint8_t section,
	const uint8_t layer,
	const uint8_t offset,
	const axis_t axis
) {
	quartet_t quartet = { 0 };
	if (!calculate_quartet(&quartet, section, layer, offset, axis))
		return false;
	uint32_t first_idx = coord_to_index(&quartet.first);
	uint32_t second_idx = coord_to_index(&quartet.second);
	uint32_t third_idx = coord_to_index(&quartet.third);
	uint32_t fourth_idx = coord_to_index(&quartet.fourth);
	char first = tensor3[first_idx];	
	char second = tensor3[second_idx];	
	char third = tensor3[third_idx];	
	char fourth = tensor3[fourth_idx];	
	tensor3[first_idx] = fourth; 
	tensor3[second_idx] = first;
	tensor3[third_idx] = second;
	tensor3[fourth_idx] = third;
	return true;
}

/**
 * @brief Rotate a layer of a cross section of a third-order tensor 90 degrees.
 * @param[in,out] buffer The third-order tensor being rotated.
 * @param[in] section The section being rotated.
 * @param[in] layer The layer to rotate.
 * @param[in] axis The axis to rotate about.
 * @return true if the rotation was successful, false otherwise
 */
static bool rotate_layer(
	tensor3_t tensor3[TENSOR3_SIZE],
	const uint8_t section,
	const uint8_t layer,
	const axis_t axis
) {
	for (uint8_t offset = 0; offset < DIMENSION - 1 - 2 * layer; offset++)
		if (!rotate_quartet(tensor3, section, layer, offset, axis))
			return false;
	return true;
}

/**
 * @brief Rotates a cross section of a third-order tensor 90 degrees.
 * @param[in,out] buffer The third-order tensor being rotated.
 * @param[in] section The section to rotate.
 * @param[in] axis The axis to rotate about.
 * @return true if the rotation was successful, false otherwise
 */
static bool rotate_section(
	tensor3_t tensor3[TENSOR3_SIZE],
	const uint8_t section,
	const axis_t axis
) {
	for (uint8_t layer = 0; layer < SECTION_SIZE / 2; layer++)
		if (!rotate_layer(tensor3, section, layer, axis))
			return false;
	return true;
}

/**
 * @brief Rotates a third-order tensor 90 degrees.
 * @param[in,out] buffer The third-order tensor to rotate.
 * @param[in] axis The axis to rotate about.
 * @return true if the rotation was successful, false otherwise
 */
static bool rotate(tensor3_t tensor3[TENSOR3_SIZE], const axis_t axis) {
	for (uint8_t section = 0; section < DIMENSION; section++) 
		if (!rotate_section(tensor3, section, axis))
			return false;
	return true;
}

int main() {
	tensor3_t tensor3[TENSOR3_SIZE];
	memset(tensor3, '.', TENSOR3_SIZE);
	for (int i = 0; i < TENSOR3_SIZE; i++)
		tensor3[i] = i + 'A';

	printf("Coordinate to index:\n");
	for (uint8_t z = 0; z < DIMENSION; z++) {
		for (uint8_t y = 0; y < DIMENSION; y++) {
			for (uint8_t x = 0; x < DIMENSION; x++) {
				const coordinate_t coord = { x, y, z };
				const uint32_t idx = coord_to_index(&coord);
				printf("%d", idx);
				if (idx < TENSOR3_SIZE - 1)
					putchar(',');
			}
		}
	}
#if 0
	printf("\n\nIndex to coordinate:\n");
	for (uint32_t i = 0; i < TENSOR3_SIZE; i++) {
		coordinate_t coord = { 0 };
		if (index_to_coord(&coord, &i))
			printf("%d,%d,%d\n", coord.x, coord.y, coord.z);
	}
#endif

	printf("\nElements:\n");
	for (uint8_t z = 0; z < DIMENSION; z++) {
		for (uint8_t y = 0; y < DIMENSION; y++) {
			for (uint8_t x = 0; x < DIMENSION; x++) {
				const coordinate_t coord = { x, y, z };
				const uint32_t idx = coord_to_index(&coord);
				const char element = tensor3[idx];
				putchar(element);
			}
			putchar('\n');
		}
		if (z < DIMENSION - 1)
			printf("\n\n");
	}
	
	printf(rotate(tensor3, AXIS_ZNEGATIVE) ? "SUCCESS\n" : "FAILURE\n");
	
	printf("\nElements:\n");
	for (uint8_t z = 0; z < DIMENSION; z++) {
		for (uint8_t y = 0; y < DIMENSION; y++) {
			for (uint8_t x = 0; x < DIMENSION; x++) {
				const coordinate_t coord = { x, y, z };
				const uint32_t idx = coord_to_index(&coord);
				const char element = tensor3[idx];
				putchar(element);
			}
			putchar('\n');
		}
		if (z < DIMENSION - 1)
			printf("\n\n");
	}
	
}

