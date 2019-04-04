//#define DEBUG
#include <iostream>
#include "Mould.hpp"
#include "GLutil.hpp"
#include <GL/freeglut.h>

int win;
size_t count;

void draw_cb() {
  static float time;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, nullptr);
  glUniform1f(0, time);
  time += 0.01;
  glutSwapBuffers();
}

void resize_cb(int w, int h) {
  glViewport(0, 0, w, h);
}

void key_cb(unsigned char key, int, int) {
  if(key == 'q')
    glutDestroyWindow(win);
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

int main(int argc, char *argv[]) {
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

  try {
    glutInit(&argc, argv);
    glutInitContextVersion(4, 5);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA);
    glutInitWindowSize(640, 480);
    glutInitWindowPosition(100, 100);
    win = glutCreateWindow("Title");

    GLutil::initGLEW();
    glutDisplayFunc(draw_cb);
    glutIdleFunc(glutPostRedisplay);
    glutReshapeFunc(resize_cb);
    glutKeyboardFunc(key_cb);

    GLutil::program program{
      GLutil::shader{"cut.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
      GLutil::shader{"cut.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};
    glUseProgram(program);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

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
    count = indices.size();

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
