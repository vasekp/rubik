#version 300 es

precision mediump float;

uniform mat4 submodel;
uniform mat4 modelview;
uniform mat4 proj;

layout(location = 0) in vec3 in_coords;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_colour;

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
