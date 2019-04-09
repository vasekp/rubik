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
    PROJ,
    TEXTURE
  };
}

namespace attribs_model {
  enum {
    COORDS,
    NORMAL,
    COLOUR
  };
}

namespace uniforms_texgen {
  enum {
    NORMAL,
    OFFSET
  };
}

namespace attribs_texgen {
  enum {
    COORDS
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
        -0.3f, glm::vec3{1, 0, 0}),
      -time, glm::vec3{0, 1, 0});
  glUseProgram(globals::prog_model);
  glBindVertexArray(globals::vao_model);
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
    Index first = face.indices[0];
    Index prev = face.indices[1];
    for(auto ix : face.indices) {
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

Volume init_shape(float size, const std::vector<Plane>& cuts) {
  Volume shape{size};
  for(const auto& plane : cuts)
    shape.cut(plane, 1);
  return shape;
}

void init_model(const Volume& shape, const std::vector<Plane>& cuts) {
  Mould m{shape};
  for(const auto& plane : cuts)
    m.cut(plane);
  
  std::vector<glm::vec3> coords{};
  std::vector<glm::vec3> normals{};
  std::vector<glm::vec4> colours{};
  std::vector<Index> indices{};

  glm::vec4 col_vals[] = {
    {1, 1, 1, 1},
    {1, 1, 0, 1}};

  for(auto& volume : m.get_volumes()) {
    ExtVolume ext(std::move(volume), 0.05);
    size_t base = coords.size();
    const auto& vertices = ext.get_vertices();
    std::copy(begin(vertices), end(vertices), std::back_inserter(coords));
    for(const auto& face : ext.get_faces()) {
      std::fill_n(std::back_inserter(normals), face.indices.size(), face.normal);
      std::fill_n(std::back_inserter(colours), face.indices.size(), col_vals[face.tag]);
    }
    append_face_list(indices, base, ext.get_faces());
    append_face_list(indices, base, ext.get_ext_faces());
  }
#ifdef DEBUG
  std::cout << '\n' << coords.size() << ' ' << normals.size() << ' ' << indices.size() << '\n';
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
  glEnableVertexAttribArray(attribs_model::COORDS);
  glVertexAttribPointer(attribs_model::COORDS, 3, GL_FLOAT, GL_FALSE, sizeof(coords[0]), nullptr);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(normals[0]), normals.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs_model::NORMAL);
  glVertexAttribPointer(attribs_model::NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), nullptr);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, colours.size() * sizeof(colours[0]), colours.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs_model::COLOUR);
  glVertexAttribPointer(attribs_model::COLOUR, 4, GL_FLOAT, GL_FALSE, sizeof(colours[0]), nullptr);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);
  globals::model_size = indices.size();
}

void init_cubemap(unsigned texUnit, const Volume& main_volume, const std::vector<Plane>& cuts) {
  GLuint framebuffer;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  constexpr GLuint texSize = 512;
  glViewport(0, 0, texSize, texSize);

  GLuint texture;
  glGenTextures(1, &texture);
  glActiveTexture(GL_TEXTURE0 + texUnit);
  glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_LOD_BIAS, -1);
  for(GLenum face : {
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_X})
    glTexImage2D(face, 0, GL_RGBA, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

  GLutil::program prog_texgen{
    GLutil::shader{"texgen.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"texgen.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file},
    GLutil::shader{"texgen.geom", GL_GEOMETRY_SHADER, GLutil::shader::from_file}};

  // main volume faces

  std::vector<Index> indices{};
  append_face_list(indices, 0, main_volume.get_faces());

  GLuint vao_texgen;
  glGenVertexArrays(1, &vao_texgen);
  glBindVertexArray(vao_texgen);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, main_volume.get_vertices().size() * sizeof(Vertex), &main_volume.get_vertices()[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs_texgen::COORDS);
  glVertexAttribPointer(attribs_texgen::COORDS, 3, GL_FLOAT, GL_FALSE, sizeof(main_volume.get_vertices()[0]), nullptr);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(Index), &indices[0], GL_STATIC_DRAW);

  // paint cuts

  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);
  glViewport(0, 0, texSize, texSize);
  glUseProgram(prog_texgen);

  glClearColor(1, 1, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendEquation(GL_MIN);

  for(const auto& plane : cuts) {
    glUniform3fv(uniforms_texgen::NORMAL, 1, glm::value_ptr(plane.normal));
    glUniform1f(uniforms_texgen::OFFSET, plane.offset);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
  }

  glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
  glProgramUniform1i(globals::prog_model, uniforms_model::TEXTURE, texUnit);

  // reset to sensible state

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBlendEquation(GL_FUNC_ADD);
  glDisable(GL_BLEND);
  glClearColor(0, 0, 0, 1);
}

int main(int argc, char *argv[]) {
  constexpr unsigned tex_cubemap = 0;

  std::vector<Plane> shape_cuts{
    {{1, 1, 1}, 2}
  };

  std::vector<Plane> cuts{
    {{1, 1, 1}, 0},
    {{-1, 1, 1}, 0},
    {{-1, 1, -1}, 0},
    {{1, 1, -1}, 0}
  };

  try {
    init_glut(argc, argv);
    GLutil::initGLEW();
    init_program();
    Volume shape = init_shape(2, shape_cuts);
    init_model(shape, cuts);
    init_cubemap(tex_cubemap, shape, cuts);

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
