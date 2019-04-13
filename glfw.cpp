//#define DEBUG
#include <iostream>
#include "rubik.hpp"
#include <GLFW/glfw3.h>

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
  Context& ctx = *static_cast<Context*>(glfwGetWindowUserPointer(window));
  if(w == 0 && h == 0)
    glfwGetFramebufferSize(window, &w, &h);
  update_proj(ctx, w, h);
}

void button_cb(GLFWwindow* window, int, int action, int) {
  Context& ctx = *static_cast<Context*>(glfwGetWindowUserPointer(window));
  if(action == GLFW_PRESS) {
    ctx.buttondown = true;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    ctx.buttondown_loc = glm::vec2{x, y};
  } else {
    ctx.buttondown = false;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    rotate_model(ctx, touch_location(window), true);
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
    Context ctx{};
    glfwSetWindowUserPointer(window, static_cast<void*>(&ctx));

    GLutil::initGLEW();
    init_program(ctx);
    Volume shape = init_shape(ctx, 2, shape_cuts);
    init_model(ctx, shape, cuts, colours);
    init_cubemap(ctx, tex_cubemap, shape, shape_cuts, cuts);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    resize_cb(window, 0, 0);
    while(!glfwWindowShouldClose(window)) {
      if(ctx.buttondown)
        rotate_model(ctx, touch_location(window), false);
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
