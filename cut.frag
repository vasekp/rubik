#version 450 core

in vec4 normal;

out vec4 color;

void main() {
  vec4 nnormal = normalize(normal);
  color = vec4(max(dot(nnormal, normalize(vec4(1, 1, -1, 0))), 0.2));
  color = mix(color, vec4(vec3(pow(max(dot(reflect(normalize(vec4(1, 1, -1, 0)), nnormal), vec4(0, 0, 1, 0)), 0), 5)), 1), 0.5);
}
