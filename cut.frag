#version 450 core

in vec4 normal;

in vec4 pos;
out vec4 color;

const float ambient_int = 0.2;
const vec4 light_dir = normalize(vec4(3, 3, -10, 0));
const float shininess = 5;
const float mix_specular = 0.3;

void main() {
  vec4 nnormal = normalize(normal);
  vec4 npos = vec4(normalize(vec3(pos)), 1);
  vec3 diffuse = vec3(max(dot(nnormal, light_dir), ambient_int));
  float specular_base = dot(-npos, nnormal) > 0 ? max(dot(-npos, -reflect(light_dir, nnormal)), 0) : 0;
  vec3 specular = vec3(pow(specular_base, shininess));
  color = vec4(mix(diffuse, specular, mix_specular), 1);
}
