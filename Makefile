all: rubik

CXXFLAGS = -std=c++17 -g -Wall -Wextra -pedantic -fno-diagnostics-show-caret
LIBS = -lGL -lGLEW -lglfw

rubik.o: rubik.cpp Mould.hpp
	g++ -c $(CXXFLAGS) $< -o $@

glfw.o: glfw.cpp Mould.hpp
	g++ -c $(CXXFLAGS) $< -o $@

rubik: rubik.o glfw.o
	g++ $^ $(LIBS) -o $@

.PHONY: all
