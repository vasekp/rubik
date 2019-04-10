#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

in uint v_tag[];
out uint g_tag;
out vec4 coords;

const mat4 proj[] = {
  {{0, 0, 0, 1},  // +X
   {0, 1, 0, 0},
   {-1, 0, 0, 0},
   {0, 0, 0, 0}},
  {{0, 0, 0, -1}, // -X
   {0, 1, 0, 0},
   {1, 0, 0, 0},
   {0, 0, 0, 0}},
  {{1, 0, 0, 0},  // +Y
   {0, 0, 0, 1},
   {0, -1, 0, 0},
   {0, 0, 0, 0}},
  {{1, 0, 0, 0},  // -Y
   {0, 0, 0, -1},
   {0, 1, 0, 0},
   {0, 0, 0, 0}},
  {{1, 0, 0, 0},  // +Z
   {0, 1, 0, 0},
   {0, 0, 0, 1},
   {0, 0, 0, 0}},
  {{-1, 0, 0, 0},  // -Z
   {0, 1, 0, 0},
   {0, 0, 0, -1},
   {0, 0, 0, 0}}};

void main() {
  int i, j;
  for(j = 0; j < 6; j++) {
    for(i = 0; i < 3; i++) {
      gl_Layer = j;
      gl_Position = vec4(1, -1, 1.1, 1) * (proj[j] * gl_in[i].gl_Position);
      coords = gl_in[i].gl_Position;
      g_tag = v_tag[i];
      EmitVertex();
    }
    EndPrimitive();
  }
}
