#version 450 core 

in vec3 coords;
out vec4 color;

void main() {
  color = vec4((coords + vec3(1))/2, 1);
}
