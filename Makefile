C = gcc
C_FLAGS = -std=c2x -Wall -Werror -pedantic -ggdb -O0
PROGRAM = 3d

$(PROGRAM): $(PROGRAM).c
	$(C) $(C_FLAGS) $< -o $@

.PHONY: clean

clean:
	rm $(PROGRAM)
