#version 300 es

precision mediump float;

uniform mat4 matrix;
uniform mat4 submodel;

layout(location = 0) in vec3 in_coords;
layout(location = 1) in vec3 in_normal;

out vec3 x_coords;
out vec3 x_normal;

void main() {
  gl_Position = matrix * submodel * vec4(in_coords, 1);
  x_coords = mat3(submodel) * in_coords;
  x_normal = mat3(submodel) * in_normal;
}
