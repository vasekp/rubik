//#define DEBUG
#include <iostream>
#include "rubik.hpp"
#include <GLFW/glfw3.h>

glm::ivec2 cursor_pos(GLFWwindow* window) {
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  return {(int)x, (int)y};
}

void resize_cb(GLFWwindow* window, int w, int h) {
  Context& ctx = *static_cast<Context*>(glfwGetWindowUserPointer(window));
  if(w == 0 && h == 0)
    glfwGetFramebufferSize(window, &w, &h);
  update_proj(ctx, w, h);
}

void button_cb(GLFWwindow* window, int, int action, int) {
  Context& ctx = *static_cast<Context*>(glfwGetWindowUserPointer(window));
  if(action == GLFW_PRESS) {
    ctx.ui.buttondown_win = cursor_pos(window);
    if(auto response = project_click(ctx, ctx.ui.buttondown_win); !response) {
      ctx.ui.rot_view = true;
    } else {
      const Piece& p = ctx.pieces[response->volume_index];
      const Volume& v = p.volume;
      auto cuts = v.get_rot_cuts();
      // Ignore rotations parallel to the touched surface and back-facing rotations
      cuts.erase(
          std::remove_if(cuts.begin(), cuts.end(), [&ctx, &p, n = response->normal](const Cut& c) {
            return std::abs(glm::dot(glm::mat3{p.rotation} * c.plane.normal, n)) > 0.99
              || (glm::mat3{ctx.mxs.view * ctx.mxs.model * p.rotation} * c.plane.normal).z < -.5;
          }), cuts.end());
      if(!cuts.empty()) {
        ctx.ui.rot_action = true;
        ctx.ui.buttondown_mod = response->coords;
        ctx.ui.action_center = v.center(); 
        ctx.ui.action_cut = cuts.back();
      } else
        ctx.ui.rot_view = true;
    }
  } else {
    if(ctx.ui.rot_view) {
      rotate_model(ctx, cursor_pos(window), true);
      ctx.ui.rot_view = false;
    } else if(ctx.ui.rot_action) {
      for(auto& piece : ctx.pieces)
        piece.rotation_temp = glm::mat4{1};
      ctx.ui.rot_action = false;
    }
  }
}

void key_cb(GLFWwindow *window, unsigned key) {
  if(key == 'q')
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void error_cb(int, const char* desc) {
  std::cout.flush();
  std::cerr << desc << '\n';
}

GLFWwindow* init_glfw() {
  if(!glfwInit())
    throw std::runtime_error("glfwInit failed\n");;
  glfwSetErrorCallback(error_cb);
  //glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  GLFWwindow* window = glfwCreateWindow(640, 480, "Title", NULL, NULL);
  if(!window)
    throw std::runtime_error("glfwCreateWindow failed");
  glfwSetCharCallback(window, key_cb);
  glfwSetFramebufferSizeCallback(window, resize_cb);
  glfwSetMouseButtonCallback(window, button_cb);
  glfwMakeContextCurrent(window);
  return window;
}

int main() {
  constexpr unsigned tex_cubemap = 0;

  std::vector<Plane> shape_cuts{};
  std::vector<Cut> cuts{};

  /*Solid shape = Solid::platonic(4, 3);
  Index ix = 0;
  float r_face = shape.r_face();
  for(const auto& [perm, vector] : shape.face_dirs()) {
    shape_cuts.push_back({{vector, r_face}, ++ix});
    cuts.push_back({vector, r_face / 3});
  }*/

  Solid shape = Solid::dihedral(3, 0.5);
  float r_edge = shape.r_edge();
  for(const auto& [perm, vector] : shape.edge_dirs()) {
    shape_cuts.push_back({vector, r_edge});
    auto transform = perm * shape.edge_perm() * perm.inverse();
    cuts.push_back({{vector, r_edge / 4}, static_cast<Index>(transform.to_numbered())});
    cuts.push_back({{-vector, r_edge / 2}, static_cast<Index>(transform.inverse().to_numbered())});
  }
  float r_face = shape.r_face();
  for(const auto& [perm, vector] : shape.face_dirs()) {
    shape_cuts.push_back({vector, r_face});
    auto transform = perm * shape.face_perm() * perm.inverse();
    cuts.push_back({{vector, 0}, static_cast<Index>(transform.to_numbered())});
  }

  try {
    GLFWwindow* window = init_glfw();
    Context ctx{};
    glfwSetWindowUserPointer(window, static_cast<void*>(&ctx));

    GLutil::initGLEW();
    init_programs(ctx);
    Volume shape = init_shape(ctx, 2, shape_cuts);
    init_model(ctx, shape, cuts, {}/*colours*/);
    init_model_basic(ctx, shape);
    init_cubemap(ctx, tex_cubemap, shape_cuts, cuts);
    init_click_target(ctx);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    resize_cb(window, 0, 0);
    while(!glfwWindowShouldClose(window)) {
      glm::vec2 cursor = cursor_pos(window);
      if(ctx.ui.rot_view)
        rotate_model(ctx, cursor, false);
      if(ctx.ui.rot_action)
        rotate_action(ctx, cursor);
      draw(ctx);
      glfwSwapBuffers(window);
      glfwPollEvents();
    }
  } catch(const std::runtime_error& e) {
    std::cout.flush();
    std::cerr << e.what() << '\n';
  }
  glfwTerminate();
}
