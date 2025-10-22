#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const uint8_t WIDTH = 4;
const uint8_t HEIGHT = 4;
const uint8_t DEPTH = 4;
const uint32_t SIZE = WIDTH * HEIGHT * DEPTH;

typedef struct {
	uint8_t x, y, z;
} coordinate_t;

uint32_t coord_to_index(const coordinate_t* const coord) {
	return coord->x + (coord->y * WIDTH) + (coord->z * WIDTH * HEIGHT);
}

bool index_to_coord(coordinate_t* const coord, const uint32_t* const idx) {
	if (*idx >= SIZE)
		return false;
	coord->x = *idx % WIDTH;
	coord->y = (*idx / WIDTH) % HEIGHT;
	coord->z = *idx / (WIDTH * HEIGHT);
	return true;
}

int main() {
	char buffer[SIZE];
	memset(buffer, '.', SIZE);

	printf("Coordinate to index:\n");
	for (uint8_t z = 0; z < DEPTH; z++) {
		for (uint8_t y = 0; y < HEIGHT; y++) {
			for (uint8_t x = 0; x < WIDTH; x++) {
				const coordinate_t coord = { x, y, z };
				const uint32_t idx = coord_to_index(&coord);
				printf("%d", idx);
				if (idx < SIZE - 1)
					putchar(',');
			}
		}
	}

	printf("\n\nIndex to coordinate:\n");
	for (uint32_t i = 0; i < SIZE; i++) {
		coordinate_t coord = { 0 };
		if (index_to_coord(&coord, &i))
			printf("%d,%d,%d\n", coord.x, coord.y, coord.z);
	}

	printf("\nElements:\n");
	for (uint8_t z = 0; z < DEPTH; z++) {
		for (uint8_t y = 0; y < HEIGHT; y++) {
			for (uint8_t x = 0; x < WIDTH; x++) {
				const coordinate_t coord = { x, y, z };
				const uint32_t idx = coord_to_index(&coord);
				const char element = buffer[idx];
				putchar(element);
			}
			putchar('\n');
		}
		if (z < DEPTH - 1)
			printf("\n\n");
	}
}

