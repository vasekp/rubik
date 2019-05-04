all: rubik

HEADERS = Mould.hpp GLutil.hpp Permutation.hpp Group.hpp Solid.hpp rubik.hpp
CXXFLAGS = -std=c++17 -g -Wall -Wextra -pedantic -fno-diagnostics-show-caret
LIBS = -lGL -lGLEW -lglfw -lm

rubik.o: rubik.cpp $(HEADERS)
	g++ -c $(CXXFLAGS) $< -o $@

glfw.o: glfw.cpp $(HEADERS)
	g++ -c $(CXXFLAGS) $< -o $@

rubik: rubik.o glfw.o
	g++ $^ $(LIBS) -o $@

.PHONY: all
