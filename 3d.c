/**
 * @file 3d.c
 * @author Justin Thoreson
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const uint8_t DIM = 4;
const uint16_t FACE_SIZE = DIM * DIM;
const uint32_t CUBE_SIZE = FACE_SIZE * DIM;

/**
 * @brief A coordinate of an element within the 3D matrix.
 */
typedef struct {
	uint8_t x, y, z;
} coordinate_t;

/**
 * @brief Four coordinates within the 3D space whose elements must be rotated.
 *
 * Rotating a square (or, rather, a cross section of a cube) cannot be done
 * in-place, as elements will be overwritten when rotating. Thus, the data must
 * be cached so it will not be overwritten. Instead of creating a copy of the
 * entire square, only four elements need to be copied simultaneously. Point
 * one takes the position of point two, point two takes the position of point
 * three, point three takes the position of point four, and point four takes
 * the position of point one. This structure tracks the coordinates of these
 * points/elements.
 */
typedef struct {
	coordinate_t first, second, third, fourth;
} quartet_t;

/**
 * @brief Enumeration of all axes.
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
 *            /|
 *           / |
 *          /  |
 * (near) -z    +y
 *
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
 * @brief Calculate the corresponding index given a coordinate.
 * @param[in] coordinate_t A coordinate structure to convert to an index.
 * @return The calculated index.
 *
 * The ordering of indices within the 3D matrix treats the width as the most
 * significant dimension, the height as the next significant, and the depth as
 * the least significant.
 *
 * Formula: index = x + (y * width) + (z * width * height)
 */
static uint32_t coord_to_index(const coordinate_t* const coord) {
	return coord->x + (coord->y * DIM) + (coord->z * FACE_SIZE);
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
static bool index_to_coord(coordinate_t* const coord, const uint32_t* const idx) {
	if (*idx >= CUBE_SIZE)
		return false;
	coord->x = *idx % DIM;
	coord->y = (*idx / DIM) % DIM;
	coord->z = *idx / FACE_SIZE;
	return true;
}

/**
 * @brief Calculates a quartet of coordinates to rotate within a 3D matrix.
 * @param[out] quartet The quartet of coordinates under calculation.
 * @param[in] section The section of the matrix.
 * @param[in] layer The layer within the section.
 * @param[in] offset The offset within the layer.
 * @param[in] axis The axis to rotate about.
 */
static void calculate_quartet(
	quartet_t* const quartet,
	const uint8_t section,
	const uint8_t layer,
	const uint8_t offset,
	const axis_t axis
) {
	switch (axis) {
		case AXIS_XPOSITIVE:
			*quartet = (quartet_t){
				.first  = { DIM - 1 - section, layer,                    layer + offset           },
				.second = { DIM - 1 - section, layer + offset,           DIM - 1 - layer          },
				.third  = { DIM - 1 - section, DIM - 1 - layer,          DIM - 1 - layer - offset },
				.fourth = { DIM - 1 - section, DIM - 1 - layer - offset, layer                    }
			};
			break;
		case AXIS_XNEGATIVE:
			// +x reversed
			*quartet = (quartet_t){
				.first  = { DIM - 1 - section, DIM - 1 - layer - offset, layer                    },
				.second = { DIM - 1 - section, DIM - 1 - layer,          DIM - 1 - layer - offset },
				.third  = { DIM - 1 - section, layer + offset,           DIM - 1 - layer          },
				.fourth = { DIM - 1 - section, layer,                    layer + offset           }
			};
			break;
		case AXIS_YPOSITIVE:
			*quartet = (quartet_t){
				.first  = { layer + offset ,          DIM - 1 - section, layer                    },
				.second = { DIM - 1 - layer,          DIM - 1 - section, layer + offset           },
				.third  = { DIM - 1 - layer - offset, DIM - 1 - section, DIM - 1 - layer          },
				.fourth = { layer,                    DIM - 1 - section, DIM - 1 - layer - offset }
			};
			break;
		case AXIS_YNEGATIVE:
			// +y reversed
			*quartet = (quartet_t){
				.first  = { layer,                    DIM - 1 - section, DIM - 1 - layer - offset },
				.second = { DIM - 1 - layer - offset, DIM - 1 - section, DIM - 1 - layer          },
				.third  = { DIM - 1 - layer,          DIM - 1 - section, layer + offset           },
				.fourth = { layer + offset ,          DIM - 1 - section, layer                    }
			};
			break;
		case AXIS_ZPOSITIVE:
			// -z reversed
			*quartet = (quartet_t){
				.first  = { layer,                    DIM - 1 - layer - offset, section },
				.second = { DIM - 1 - layer - offset, DIM - 1 - layer,          section },
				.third  = { DIM - 1 - layer,          layer + offset,           section },
				.fourth = { layer + offset,           layer,                    section }
			};	
			break;
		case AXIS_ZNEGATIVE:
			*quartet = (quartet_t){
				.first  = { layer + offset,           layer,                    section },
				.second = { DIM - 1 - layer,          layer + offset,           section },
				.third  = { DIM - 1 - layer - offset, DIM - 1 - layer,          section },
				.fourth = { layer,                    DIM - 1 - layer - offset, section }
			};	
			break;
		default:
			// return false
			*quartet = (quartet_t){ 0 };
			break;
	}
	//return true;
}

/**
 * @brief Rotates a quartet of coordinates within a layer of a 3D matrix.
 * @param[in,out] buffer The 3D matrix being rotated.
 * @param[in] section The section being rotated.
 * @param[in] layer The layer being rotated.
 * @param[in] offset The offset within the layer used to calculate the quartet.
 * @param[in] axis The axis to rotate about.
 */
static void rotate_quartet(
	char buffer[CUBE_SIZE],
	const uint8_t section,
	const uint8_t layer,
	const uint8_t offset,
	const axis_t axis
) {
	quartet_t quartet = { 0 };
	calculate_quartet(&quartet, section, layer, offset, axis);
	uint32_t first_idx = coord_to_index(&quartet.first);
	uint32_t second_idx = coord_to_index(&quartet.second);
	uint32_t third_idx = coord_to_index(&quartet.third);
	uint32_t fourth_idx = coord_to_index(&quartet.fourth);
	char first = buffer[first_idx];	
	char second = buffer[second_idx];	
	char third = buffer[third_idx];	
	char fourth = buffer[fourth_idx];	
	buffer[first_idx] = fourth; 
	buffer[second_idx] = first;
	buffer[third_idx] = second;
	buffer[fourth_idx] = third;
}

/**
 * @brief Rotates a layer of a cross section of a 3D matrix 90 degrees.
 * @param[in,out] buffer The 3D matrix being rotated.
 * @param[in] section The section being rotated.
 * @param[in] layer The layer to rotate.
 * @param[in] axis The axis to rotate about.
 */
static void rotate_layer(
	char buffer[CUBE_SIZE],
	const uint8_t section,
	const uint8_t layer,
	const axis_t axis
) {
	for (uint8_t offset = 0; offset < DIM - 1 - 2 * layer; offset++)
		rotate_quartet(buffer, section, layer, offset, axis);
}

/**
 * @brief Rotates a cross section of a 3D matrix 90 degrees.
 * @param[in,out] buffer The 3D matrix being rotated.
 * @param[in] section The section to rotate.
 * @param[in] axis The axis to rotate about.
 *
 *
 */
static void rotate_section(
	char buffer[CUBE_SIZE],
	const uint8_t section,
	const axis_t axis
) {
	for (uint8_t layer = 0; layer < FACE_SIZE / 2; layer++)
		rotate_layer(buffer, section, layer, axis);
}

/**
 * @brief Rotates a 3D matrix 90 degrees.
 * @param[in,out] buffer The 3D matrix to rotate.
 * @param[in] axis The axis to rotate about.
 */
static void rotate(char buffer[CUBE_SIZE], const axis_t axis) {
	for (uint8_t section = 0; section < DIM; section++) 
		rotate_section(buffer, section, axis);
}

int main() {
	char buffer[CUBE_SIZE];
	memset(buffer, '.', CUBE_SIZE);
	for (int i = 0; i < CUBE_SIZE; i++)
		buffer[i] = i + 'A';

	printf("Coordinate to index:\n");
	for (uint8_t z = 0; z < DIM; z++) {
		for (uint8_t y = 0; y < DIM; y++) {
			for (uint8_t x = 0; x < DIM; x++) {
				const coordinate_t coord = { x, y, z };
				const uint32_t idx = coord_to_index(&coord);
				printf("%d", idx);
				if (idx < CUBE_SIZE - 1)
					putchar(',');
			}
		}
	}

	printf("\n\nIndex to coordinate:\n");
	for (uint32_t i = 0; i < CUBE_SIZE; i++) {
		coordinate_t coord = { 0 };
		if (index_to_coord(&coord, &i))
			printf("%d,%d,%d\n", coord.x, coord.y, coord.z);
	}

	printf("\nElements:\n");
	for (uint8_t z = 0; z < DIM; z++) {
		for (uint8_t y = 0; y < DIM; y++) {
			for (uint8_t x = 0; x < DIM; x++) {
				const coordinate_t coord = { x, y, z };
				const uint32_t idx = coord_to_index(&coord);
				const char element = buffer[idx];
				putchar(element);
			}
			putchar('\n');
		}
		if (z < DIM - 1)
			printf("\n\n");
	}
	
	rotate(buffer, AXIS_XNEGATIVE);
	
	printf("\nElements:\n");
	for (uint8_t z = 0; z < DIM; z++) {
		for (uint8_t y = 0; y < DIM; y++) {
			for (uint8_t x = 0; x < DIM; x++) {
				const coordinate_t coord = { x, y, z };
				const uint32_t idx = coord_to_index(&coord);
				const char element = buffer[idx];
				putchar(element);
			}
			putchar('\n');
		}
		if (z < DIM - 1)
			printf("\n\n");
	}
	
}

