#version 300 es

precision mediump float;

uniform mat4 submodel;
uniform mat4 modelview;
uniform mat4 proj;

in vec3 in_coords;
in vec3 in_normal;
in vec4 in_colour;

out vec4 coords;
out vec3 texCoord;
out vec4 normal;
out vec4 faceColour;

void main() {
  coords = modelview * submodel * vec4(in_coords, 1);
  normal = modelview * submodel * vec4(in_normal, 0);
  texCoord = in_coords;
  faceColour = in_colour;
  gl_Position = proj * coords;
}
