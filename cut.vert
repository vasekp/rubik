#version 450 core

layout(location = 0) uniform float t;

layout(location = 0) in vec3 coord;
layout(location = 1) in vec3 in_normal;

out vec4 pos;
out vec4 normal;

void main() {
  mat3 r = {{1, 0, 0}, {0, cos(.2), -sin(.2)}, {0, sin(.2), cos(.2)}};
  mat3 s = {{cos(t), 0, sin(t)}, {0, 1, 0}, {-sin(t), 0, cos(t)}};
  gl_Position = vec4(s*r*coord/4, 1);
  pos = gl_Position + vec4(0, 0, 3, 0);
  normal = vec4(s*r*in_normal, 0);
}
