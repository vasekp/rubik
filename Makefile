all: cut

cut: cut.cpp Mould.hpp
	g++ -std=c++17 -g -Wall -Wextra -pedantic -fno-diagnostics-show-caret -lOpenGL -lGLEW -lglut cut.cpp -o cut

.PHONY: all
