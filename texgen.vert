#version 450 core

const vec2 coords[] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};

void main() {
  gl_Position = vec4(coords[gl_VertexID], 0, 1);
}
