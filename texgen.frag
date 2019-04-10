#version 450 core

layout(location = 0) uniform vec3 normal;
layout(location = 1) uniform float off;
layout(location = 2) uniform uint tag;

in vec4 coords;
in flat uint g_tag;
out float color;

float cutoff(float q) {
  return abs(q) < 0.04 ? 0 : 1;
}

void main() {
  if(g_tag == tag)
    discard;
  color = cutoff(dot(normal, vec3(coords)) - off);
}
