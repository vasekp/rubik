#version 450 core

layout(location = 0) in vec3 coords;

void main() {
  gl_Position = vec4(coords, 1);
}
