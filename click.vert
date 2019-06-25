#version 300 es

precision mediump float;

uniform mat4 matrix;
uniform mat4 submodel;
uniform int u_tag;
uniform vec2 loc;

layout(location = 0) in vec3 coords;

flat out int tag;

void main() {
  vec4 pos = matrix * submodel * vec4(coords, 1);
  pos.xy /= pos.w;
  pos.xy -= loc;
  pos.w = 1.f;
  gl_Position = pos;
  tag = u_tag;
}
