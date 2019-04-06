all: cut

cut: cut.cpp Mould.hpp
	g++ -std=c++17 -g -Wall -Wextra -pedantic -fno-diagnostics-show-caret -lGL -lGLEW -lglfw cut.cpp -o cut

.PHONY: all
