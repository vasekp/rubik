#version 450 core

layout(location = 0) uniform vec3 normal;
layout(location = 1) uniform float off;

in vec4 coords;
out vec4 color;

float cutoff(float q) {
  return abs(q) < 0.03 ? 0.3 : 1;
}

void main() {
  color = vec4(cutoff(dot(normal, vec3(coords)) - off));
}
