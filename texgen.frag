#version 300 es

precision mediump float;

uniform vec3 normal;
uniform float off;
uniform uint u_tag;

in vec3 coords;
flat in uint tag;
out vec3 color;

float cutoff(float q) {
  return clamp((abs(q)-0.04)*300., 0., 1.);
}

void main() {
  if(tag == u_tag)
    discard;
  color = vec3(cutoff(dot(normal, coords) - off));
}
