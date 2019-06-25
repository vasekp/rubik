#version 300 es

precision mediump float;

uniform mat4 matrix;
uniform mat4 submodel;
uniform vec2 loc;

layout(location = 0) in vec3 in_coords;
layout(location = 1) in vec3 in_normal;

out vec3 x_coords;
out vec3 x_normal;

void main() {
  vec4 pos = matrix * submodel * vec4(in_coords, 1);
  pos.xy /= pos.w;
  pos.xy -= loc;
  pos.w = 1.f;
  gl_Position = pos;
  x_coords = in_coords;
  x_normal = in_normal;
}
