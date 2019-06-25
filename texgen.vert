#version 300 es

precision highp float;

uniform mat4 proj;

layout(location = 0) in vec3 in_coords;
layout(location = 1) in uint in_tag;
layout(location = 2) in vec3 in_normal;

out vec3 coords;
flat out uint f_tag;
flat out vec3 f_normal;

uniform vec3 p_normal;
flat out float sine, cosine;

void main() {
  vec4 c1 = vec4(in_coords, 1);
  gl_Position = vec4(1, -1, 1, 1) * (proj * c1);
  coords = vec3(c1);
  f_tag = in_tag;
  f_normal = in_normal;
  cosine = dot(f_normal, p_normal);
  sine = sqrt(1. - pow(cosine, 2.));
}
