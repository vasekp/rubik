#version 450 core

layout(location = 1) uniform vec3 normal;
layout(location = 2) uniform float off;
layout(location = 3) uniform uint u_tag;

in vec3 coords;
in flat uint tag;
out float color;

float cutoff(float q) {
  return abs(q) < 0.04 ? 0 : 1;
}

void main() {
  if(tag == u_tag)
    discard;
  color = cutoff(dot(normal, coords) - off);
}
