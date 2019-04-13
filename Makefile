all: rubik

HEADERS = Mould.hpp GLutil.hpp rubik.hpp
CXXFLAGS = -std=c++17 -g -Wall -Wextra -pedantic -fno-diagnostics-show-caret
LIBS = -lGL -lGLEW -lglfw

rubik.o: rubik.cpp $(HEADERS)
	g++ -c $(CXXFLAGS) $< -o $@

glfw.o: glfw.cpp $(HEADERS)
	g++ -c $(CXXFLAGS) $< -o $@

rubik: rubik.o glfw.o
	g++ $^ $(LIBS) -o $@

.PHONY: all
