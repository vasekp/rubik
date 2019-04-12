#version 300 es

precision mediump float;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

in vec3 in_coords;
in vec3 in_normal;
in vec4 in_colour;

out vec4 coords;
out vec3 texCoord;
out vec4 normal;
out vec4 faceColour;

void main() {
  coords = view * model * vec4(in_coords, 1);
  normal = view * model * vec4(in_normal, 0);
  texCoord = in_coords;
  faceColour = in_colour;
  gl_Position = proj * coords;
}
