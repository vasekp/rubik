#version 300 es

precision lowp float;

//uniform mat4 mx;
layout(location = 0) in vec2 pos;

void main() {
  gl_Position = vec4(pos, 0, 1); //mx * vec4(pos, 1);
}
