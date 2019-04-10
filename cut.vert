#version 450 core

layout(location = 0) uniform mat4 modelview;
layout(location = 1) uniform mat4 proj;
layout(location = 3) uniform uint in_volID;

layout(binding = 0) buffer Mxs {
  mat4 mx[];
};

layout(location = 0) in vec3 in_coord;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_colour;

out vec4 coords;
out vec3 texCoord;
out vec4 normal;
out vec4 faceColour;

void main() {
  coords = modelview * mx[in_volID] * vec4(in_coord, 1);
  texCoord = in_coord;
  gl_Position = proj * coords;
  normal = modelview * vec4(in_normal, 0);
  faceColour = in_colour;
}
