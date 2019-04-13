//#define DEBUG
#include <iostream>
#include "Mould.hpp"
#include "GLutil.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace globals {
  extern GLuint vao_model;
  extern GLutil::program prog_model;
  extern GLuint all_matrices;
  extern GLuint this_matrix;
  extern std::vector<Vertex> centers;
  extern std::vector<GLvoid*> starts;
  extern std::vector<GLsizei> counts;
  extern std::vector<glm::mat4> matrices;
  extern glm::vec2 buttondown_loc;
  extern glm::vec2 window_size;
  extern bool buttondown;
  extern glm::mat4 proj;
  extern glm::mat4 view;
  extern glm::mat4 model;
  extern struct UM {
    GLint submodel;
    GLint view;
    GLint proj;
    GLint texture;
  } uniforms_model;
  extern size_t piece_count;
}

struct Cut {
  Plane plane;
  Index tag;
};

void init_program();
Volume init_shape(float size, const std::vector<Cut>& cuts);
void init_model(const Volume& shape, const std::vector<Plane>& cuts, const std::vector<glm::vec4>& colour_vals);
void init_cubemap(unsigned texUnit, const Volume& main_volume, const std::vector<Cut>& shape_cuts, const std::vector<Plane>& cuts);

void update_proj(int w, int h);
void rotate_model(glm::vec2 loc, bool rewrite);

void draw();

#ifdef DEBUG
void report(const std::string& r) {
  std::cout << std::hex;
  while(auto e = glGetError()) {
    std::cout << r << ": " << e << '\n';
  }
}
#endif

glm::vec2 touch_location(GLFWwindow* window) {
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  return glm::vec2{x, y};
}

void resize_cb(GLFWwindow* window, int w, int h) {
  if(w == 0 && h == 0)
    glfwGetFramebufferSize(window, &w, &h);
  update_proj(w, h);
}

void button_cb(GLFWwindow* window, int, int action, int) {
  if(action == GLFW_PRESS) {
    globals::buttondown = true;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    globals::buttondown_loc = glm::vec2{x, y};
  } else {
    globals::buttondown = false;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    rotate_model(touch_location(window), true);
  }
}

void key_cb(GLFWwindow *window, unsigned key) {
  if(key == 'q')
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

GLFWwindow* init_glfw() {
  if(!glfwInit())
    throw std::runtime_error("glfwInit failed\n");;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
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

  std::vector<Cut> shape_cuts{
    {{{1, 0, 0}, 1}, 1},
    {{{0, 1, 0}, 1}, 2},
    {{{0, 0, 1}, 1}, 3},
    {{{-1, 0, 0}, 1}, 4},
    {{{0, -1, 0}, 1}, 5},
    {{{0, 0, -1}, 1}, 6},
    {{{1, 1, 1}, 1}, 7}
  };

  std::vector<Plane> cuts{
    {{1, 1, 1}, 0},
    {{-1, 1, 1}, 0},
    {{-1, 1, -1}, 0},
    {{1, 1, -1}, 0}
  };

  std::vector<glm::vec4> colours{
    {0, 0, 0, 0},
    {1, 0, 0, 1},
    {0, 1, 0, 1},
    {0, 0, 1, 1},
    {1, 0, 0, 1},
    {0, 1, 0, 1},
    {0, 0, 1, 1},
    {1, 1, 0, 1},
  };

  try {
    GLFWwindow* window = init_glfw();
    GLutil::initGLEW();
    init_program();
    Volume shape = init_shape(2, shape_cuts);
    init_model(shape, cuts, colours);
    init_cubemap(tex_cubemap, shape, shape_cuts, cuts);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    resize_cb(window, 0, 0);
    while(!glfwWindowShouldClose(window)) {
      if(globals::buttondown)
        rotate_model(touch_location(window), false);
      draw();
      glfwSwapBuffers(window);
      glfwPollEvents();
    }
  } catch(const std::runtime_error& e) {
    std::cout.flush();
    std::cerr << e.what() << '\n';
  }
  glfwTerminate();
}
