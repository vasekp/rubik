#version 300 es

precision highp float;

// Cutting plane properties
uniform vec3 p_normal;
uniform float p_offset;
uniform uint p_tag;

// Face properties
flat in uint f_tag;

in vec3 coords;
out float color;
flat in float sine, cosine;

const float bevel_radius = 0.03;
const float extra_border = 0.02;

void main() {
  if(f_tag == p_tag)
    discard;
  float q1 = dot(p_normal, coords) - p_offset;
  float q2 = max(abs(q1 - bevel_radius * cosine) - bevel_radius, 0.) / sine;
  color = sign(q1) * pow(min(q2 / extra_border, 1.), 10.);
}
