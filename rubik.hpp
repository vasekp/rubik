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
    bool rot_view;
    bool rot_action;
    glm::vec3 action_center;
    std::optional<Cut> action_cut;
  } ui;
  std::vector<Piece> pieces;
};

void init_programs(Context& ctx);
Volume init_shape(Context& ctx, float size, const std::vector<Plane>& shape_cuts);
void init_model(Context& ctx, const Volume& shape, const std::vector<Cut>& cuts, const std::vector<glm::vec4>& colour_vals);
void init_model_basic(Context& ctx, Volume shape);
void init_cubemap(Context& ctx, unsigned texUnit, const std::vector<Plane>& shape_cuts, const std::vector<Cut>& cuts);
void init_click_target(Context& ctx);

void update_proj(Context& ctx, int w, int h);
void rotate_model(Context& ctx, glm::ivec2 cursor_wnd, bool rewrite);
void rotate_action(Context& ctx, glm::ivec2 cursor_wnd);

struct click_response {
  Index volume_index;
  glm::vec3 coords;
  glm::vec3 normal;
};
std::optional<click_response> project_click(Context& ctx, glm::ivec2 point);

void draw(Context& ctx);

template<>
struct element_traits<glm::mat4> {
  static glm::mat4 identity() {
    return glm::mat4{1};
  }
};

#endif
