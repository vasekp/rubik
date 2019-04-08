#version 450 core

layout(location = 2) uniform samplerCube sampler;

in vec4 pos;
in vec3 texCoord;
in vec4 normal;
out vec4 color;

const float ambient_int = 0.3;
const vec4 light_dir = normalize(vec4(3, 3, -10, 0));
const float shininess = 5;
const float mix_specular = 0.3;

void main() {
  vec4 proper_color = texture(sampler, texCoord);
  vec4 nnormal = normalize(normal);
  vec4 npos = vec4(normalize(vec3(pos)), 1);
  vec3 diffuse = (max(dot(nnormal, light_dir), 0) + ambient_int) * vec3(proper_color);
  float specular_base = dot(-npos, nnormal) > 0 ? max(dot(-npos, -reflect(light_dir, nnormal)), 0) : 0;
  vec3 specular = vec3(pow(specular_base, shininess));
  color = vec4(mix(diffuse, specular, mix_specular), 1);
}
