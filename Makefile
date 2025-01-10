.PHONY: default clean format test examples-and-tests compile_commands.json

MAIN = main

SRCS = $(shell find ./ -not -name "v4l2_capture_example.c" -and -name "*.c")
HDRS = $(shell find ./ -name "*.h")

CC       := gcc
CFLAGS   := -std=gnu23 -pedantic -g -Wall -Wextra -msse3 -Ofast -fno-finite-math-only -march=native
LFLAGS   := -lm $$(pkg-config --libs glfw3 opengl glu glew)
INCLUDES := -I.
LIBS     :=

default: $(MAIN)

$(MAIN): main.c frame_reader.h stb_image.h
	@mkdir -p $$(dirname $(MAIN))
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(LFLAGS) $(LIBS) main.c

v4l2_capture_example: v4l2_capture_example.c
	$(CC)  -o v4l2_capture_example v4l2_capture_example.c

format:
	clang-format -style=file -i main.c
	clang-format -style=file -i frame_reader.h

clean:
	rm -rf $(MAIN)
	rm -rf **/*.o
	rm -rf bin

compile_commands.json:
	make --always-make --dry-run | grep -wE 'gcc|g\+\+|c\+\+' | grep -w '\-c' | sed 's|cd.*.\&\&||g' | jq -nR '[inputs|{directory:"'`pwd`'", command:., file: (match(" [^ ]+$$").string[1:-1] + "c")}]' > compile_commands.json

