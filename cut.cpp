//#define DEBUG
#include <iostream>
#include "Mould.hpp"
#include "GLutil.hpp"
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace globals {
  int window;
  size_t model_size;
  GLuint vao_model;
  GLutil::program prog_model;
}

namespace uniforms_model {
  enum {
    MODELVIEW,
    PROJ
  };
}

void draw_cb() {
  static float time;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glm::mat4 modelview = glm::rotate(
      glm::rotate(
        glm::scale(
          glm::translate(
            glm::mat4{},
            glm::vec3(0, 0, 3)),
          glm::vec3(.5)),
        0.2f, glm::vec3{1, 0, 0}),
      -time, glm::vec3{0, 1, 0});
  glUseProgram(globals::prog_model);
  glUniformMatrix4fv(uniforms_model::MODELVIEW, 1, GL_FALSE, glm::value_ptr(modelview));
  glDrawElements(GL_TRIANGLES, globals::model_size, GL_UNSIGNED_SHORT, nullptr);
  glutSwapBuffers();
  time += 0.01;
}

void resize_cb(int w, int h) {
  glViewport(0, 0, w, h);
  float ratio = (float)w / h;
  glm::mat4 proj{
    {w > h ? 1/ratio : 1, 0, 0, 0},
    {0, w < h ? ratio : 1, 0, 0},
    {0, 0, .1, .2},
    {0, 0, 0, 1}};
  glProgramUniformMatrix4fv(globals::prog_model, uniforms_model::PROJ, 1, GL_FALSE, glm::value_ptr(proj));
}

void key_cb(unsigned char key, int, int) {
  if(key == 'q')
    glutDestroyWindow(globals::window);
}

void append_face_list(std::vector<Index>& indices, size_t base, const std::vector<Face>& faces) {
  for(const auto& face : faces) {
    Index first = face.vertices[0];
    Index prev = face.vertices[1];
    for(auto ix : face.vertices) {
      if(ix == first || ix == prev)
        continue;
      indices.push_back(base + first);
      indices.push_back(base + prev);
      indices.push_back(base + ix);
      prev = ix;
    }
  }
}

void init_glut(int &argc, char *argv[]) {
  glutInit(&argc, argv);
  glutInitContextVersion(4, 5);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA);
  glutInitWindowSize(640, 480);
  glutInitWindowPosition(100, 100);
  globals::window = glutCreateWindow("Title");
  glutDisplayFunc(draw_cb);
  glutIdleFunc(glutPostRedisplay);
  glutReshapeFunc(resize_cb);
  glutKeyboardFunc(key_cb);
}

void init_program() {
  globals::prog_model = GLutil::program{
    GLutil::shader{"cut.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"cut.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};
}

void init_model() {
  Mould m{};
  m.cut({glm::normalize(glm::vec3{1, 1, 1}), 0}, false);
  m.cut({glm::normalize(glm::vec3{-1, 1, 1}), 0}, false);
  m.cut({glm::normalize(glm::vec3{-1, 1, -1}), 0}, false);
  m.cut({glm::normalize(glm::vec3{1, 1, -1}), 0}, false);

  auto volumes = m.get_volumes_clipped(0.05);
  std::vector<glm::vec3> coords{};
  std::vector<glm::vec3> normals{};
  std::vector<Index> indices{};
  {
    size_t vx_total = 0, ix_total = 0, face_count = 0;
    for(const auto& volume : volumes) {
      vx_total += volume.vertex_coords.size();
      face_count += volume.ext_faces.size() + volume.ext_edges.size() + volume.ext_vertices.size();
      for(const auto& face : volume.ext_faces)
        ix_total += (face.vertices.size() - 2)*3;
      for(const auto& face : volume.ext_edges)
        ix_total += (face.vertices.size() - 2)*3;
      for(const auto& face : volume.ext_vertices)
        ix_total += (face.vertices.size() - 2)*3;
    }
    coords.reserve(vx_total);
    normals.reserve(vx_total);
    indices.reserve(ix_total);
  }
  for(auto& volume : volumes) {
    using MoveIter = std::move_iterator<decltype(begin(volume.vertex_coords))>;
    size_t base = coords.size();
    std::copy(MoveIter(begin(volume.vertex_coords)), MoveIter(end(volume.vertex_coords)),
        std::back_inserter(coords));
    for(const auto& face : volume.ext_faces)
      std::fill_n(std::back_inserter(normals), face.vertices.size(), face.normal);
    append_face_list(indices, base, volume.ext_faces);
    append_face_list(indices, base, volume.ext_edges);
    append_face_list(indices, base, volume.ext_vertices);
  }
#ifdef DEBUG
  std::cout << coords.size() << ' ' << normals.size() << ' ' << indices.size() << '\n';
  std::cout << "[ ";
  for(auto ix : indices)
    std::cout << ix << ", ";
  std::cout << "]\n";
#endif

  glGenVertexArrays(1, &globals::vao_model);
  glBindVertexArray(globals::vao_model);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, coords.size() * sizeof(coords[0]), coords.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(coords[0]), nullptr);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(normals[0]), normals.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), nullptr);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);
  globals::model_size = indices.size();
}

int main(int argc, char *argv[]) {
  try {
    init_glut(argc, argv);
    GLutil::initGLEW();
    init_program();
    init_model();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glutMainLoop();
  } catch(const std::runtime_error& e) {
    std::cout.flush();
    std::cerr << e.what() << '\n';
  }
}
