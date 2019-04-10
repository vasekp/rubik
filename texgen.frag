#version 450 core

layout(location = 0) uniform vec3 normal;
layout(location = 1) uniform float off;
layout(location = 2) uniform uint tag;

in vec4 coords;
in flat uint g_tag;
out vec4 color;

float cutoff(float q) {
  return abs(q) < 0.04 ? 0.3 : 1;
}

void main() {
  if(g_tag == tag)
    discard;
  color = vec4(cutoff(dot(normal, vec3(coords)) - off));
}
