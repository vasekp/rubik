#version 300 es

precision highp float;

uniform vec3 normal;
uniform float off;
uniform uint u_tag;

in vec3 coords;
flat in uint tag;
out float color;

void main() {
  if(tag == u_tag)
    discard;
  float q = 20.*(dot(normal, coords) - off);
  color = sign(q) * pow(abs(clamp(q, -1., 1.)), 10.);
}
