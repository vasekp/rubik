#version 300 es

precision lowp float;

uniform vec4 u_colour;

layout(location = 0) out vec4 colour;

void main() {
  colour = u_colour;
}
