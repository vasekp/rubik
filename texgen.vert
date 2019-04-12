#version 300 es

precision mediump float;

uniform mat4 proj;

in vec3 in_coords;
in uint in_tag;

out vec3 coords;
flat out uint tag;

void main() {
  vec4 c1 = vec4(in_coords, 1);
  gl_Position = vec4(1, -1, 1, 1) * (proj * c1);
  coords = vec3(c1);
  tag = in_tag;
}
