#version 300 es

precision highp float;

uniform samplerCube sampler;
uniform bool highlight;

in vec4 coords;
in vec3 texCoord;
in vec4 normal;
in vec4 faceColour;

layout(location = 0) out vec4 colour;

const float ambient_int = 0.3;
const vec4 light_dir = normalize(vec4(3, 3, -10, 0));
const float shininess = 5.0;
const float mix_specular = 0.3;
const float base_brightness = 0.15;

void main() {
  float mask = abs(
      texture(sampler, texCoord).r +
      texture(sampler, texCoord + .5*dFdx(texCoord)).r +
      texture(sampler, texCoord - .5*dFdx(texCoord)).r +
      texture(sampler, texCoord + .5*dFdy(texCoord)).r +
      texture(sampler, texCoord - .5*dFdy(texCoord)).r) / 5.;
  vec4 properColour = mask * faceColour;
  properColour = mix(properColour, vec4(vec3(base_brightness), 1.0), 1.0 - properColour.a);
  vec4 nnormal = normalize(normal);
  vec4 npos = vec4(normalize(vec3(coords)), 1);
  float diffuse_int = (max(dot(nnormal, light_dir), 0.0) + ambient_int);
  vec3 diffuse = diffuse_int * vec3(properColour);
  float specular_base = dot(-npos, nnormal) > 0.0 ? max(dot(-npos, -reflect(light_dir, nnormal)), 0.0) : 0.0;
  vec3 specular = vec3(pow(specular_base, shininess));
  colour = vec4(mix(diffuse, specular, mix_specular), 1);
  if(highlight)
    colour.rgb *= 2.f;
}
