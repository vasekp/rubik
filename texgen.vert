#version 300 es

precision highp float;

uniform mat4 proj;

layout(location = 0) in vec3 in_coords;
layout(location = 1) in vec3 in_normal;
layout(location = 3) in uint in_tag;

out vec3 coords;
flat out uint f_tag;

uniform vec3 p_normal;
flat out float sine, cosine;

void main() {
  vec4 c1 = vec4(in_coords, 1);
  gl_Position = vec4(1, -1, 1, 1) * (proj * c1);
  coords = vec3(c1);
  f_tag = in_tag;
  cosine = dot(in_normal, p_normal);
  sine = sqrt(1. - pow(cosine, 2.));
}
