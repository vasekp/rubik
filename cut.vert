#version 450 core

layout(location = 0) uniform mat4 modelview;
layout(location = 1) uniform mat4 proj;

layout(location = 0) in vec3 coord;
layout(location = 1) in vec3 in_normal;

out vec4 pos;
out vec3 texCoord;
out vec4 normal;

void main() {
  pos = modelview * vec4(coord, 1);
  texCoord = coord;
  gl_Position = proj * pos;
  normal = modelview * vec4(in_normal, 0);
}
