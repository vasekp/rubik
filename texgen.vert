#version 450 core
layout(location = 0) uniform mat4 proj;

layout(location = 0) in vec3 in_coords;
layout(location = 1) in uint in_tag;

out vec3 coords;
out uint tag;

void main() {
  vec4 c1 = vec4(in_coords, 1);
  gl_Position = vec4(1, -1, 1, 1) * (proj * c1);
  coords = vec3(c1);
  tag = in_tag;
}
