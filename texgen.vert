#version 450 core

layout(location = 0) in vec3 in_coords;
layout(location = 1) in uint in_tag;

out uint v_tag;

void main() {
  gl_Position = vec4(in_coords, 1);
  v_tag = in_tag;
}
