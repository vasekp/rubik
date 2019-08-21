#ifndef RUBIK_HPP
#define RUBIK_HPP

#include "Mould.hpp"
#include "GLutil.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Solid.hpp"
#include <optional>
#include <array>

struct Piece {
  Volume volume;
  glm::mat4 rotation;
  glm::mat4 rotation_temp;
  GLvoid* gl_start;
  GLsizei gl_count;
};

struct Context {
  struct {
    GLuint vao_full;
    GLuint vao_basic;
    GLsizei basic_size;
    GLutil::program prog_model;
    GLutil::program prog_click;
    GLutil::program prog_trivial;
    struct {
      GLint submodel;
      GLint modelview;
      GLint proj;
      GLint texture;
    } uniforms_model;
    struct {
      GLint matrix;
      GLint submodel;
      GLint volume;
    } uniforms_click;
    GLuint fb_click;
    struct {
      GLsizei w;
      GLsizei h;
    } viewport;
  } gl;
  struct {
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 model;
  } mxs;
  struct {
    glm::ivec2 buttondown_win;
    glm::vec3 buttondown_mod;
    glm::vec3 normal;
    bool rot_view;
    bool rot_action;
    glm::vec3 action_center;
    std::vector<std::pair<Cut, glm::vec2>> action_cuts;
  } ui;
  std::vector<Piece> pieces;
};

void init_programs(Context& ctx);
Volume init_shape(Context& ctx, float size, const std::vector<Plane>& shape_cuts);
void init_model(Context& ctx, const Volume& shape, const std::vector<Cut>& cuts, const std::vector<glm::vec4>& colour_vals);
void init_model_basic(Context& ctx, Volume shape);
void init_cubemap(Context& ctx, unsigned texUnit, const std::vector<Plane>& shape_cuts, const std::vector<Cut>& cuts);
void init_click_target(Context& ctx);

void draw(const Context& ctx);
void update_proj(Context& ctx, int w, int h);
void touch_start(Context& ctx, glm::ivec2 coords);
void touch_move(Context& ctx, glm::ivec2 coords);
void touch_release(Context& ctx, glm::ivec2 coords);

#endif
