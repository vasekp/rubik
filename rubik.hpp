#ifndef RUBIK_HPP
#define RUBIK_HPP

#include "Mould.hpp"
#include "GLutil.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Piece {
  Volume volume;
  Vertex center;
  glm::mat4 rotation;
  GLvoid* gl_start;
  GLsizei gl_count;
};

struct Context {
  struct {
    GLuint vao_model;
    GLutil::program prog_model;
    struct {
      GLint submodel;
      GLint modelview;
      GLint proj;
      GLint texture;
    } uniforms_model;
  } gl;
  struct {
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 model;
  } mxs;
  struct {
    glm::vec2 buttondown_loc;
    glm::vec2 window_size;
    bool buttondown;
  } ui;
  std::vector<Piece> pieces;
};

void init_program(Context& ctx);
Volume init_shape(Context& ctx, float size, const std::vector<Cut>& cuts);
void init_model(Context& ctx, const Volume& shape, const std::vector<Plane>& cuts, const std::vector<glm::vec4>& colour_vals);
void init_cubemap(Context& ctx, unsigned texUnit, const Volume& main_volume, const std::vector<Cut>& shape_cuts, const std::vector<Plane>& cuts);

void update_proj(Context& ctx, int w, int h);
void rotate_model(Context& ctx, glm::vec2 loc, bool rewrite);

void draw(Context& ctx);

#endif
