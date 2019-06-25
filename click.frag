#version 300 es

precision mediump float;

uniform int volume;

in vec3 x_coords;
in vec3 x_normal;

layout(location = 0) out int out_volume;
layout(location = 1) out vec3 out_coords;
layout(location = 2) out vec3 out_normal;

void main() {
  out_volume = volume;
  out_coords = x_coords;
  out_normal = x_normal;
}
