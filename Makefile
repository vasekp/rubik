all: rubik

HEADERS = Mould.hpp GLutil.hpp Permutation.hpp Group.hpp Solid.hpp rubik.hpp
CXXFLAGS = -std=c++17 -g -Wall -Wextra -pedantic -fno-diagnostics-show-caret -fdiagnostics-color=auto
LIBS = -lGL -lGLEW -lglfw -lm
OBJECTS = rubik.o Volume.o glfw.o

$(OBJECTS):%.o: %.cpp $(HEADERS)
	g++ -c $(CXXFLAGS) $< -o $@

rubik: $(OBJECTS)
	g++ $^ $(LIBS) -o $@

.PHONY: all
