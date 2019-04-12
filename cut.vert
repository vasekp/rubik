#version 450 core

layout(location = 0) uniform mat4 model;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 proj;

layout(location = 0) in vec3 in_coords;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_colour;

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
