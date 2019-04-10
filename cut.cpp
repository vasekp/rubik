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
  GLuint vao_model;
  GLutil::program prog_model;
  std::vector<GLvoid*> starts;
  std::vector<GLsizei> counts;
  float time = 0;
}

namespace uniforms_model {
  enum {
    MODELVIEW,
    PROJ,
    TEXTURE,
    VOL_ID
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
    PROJ,
    NORMAL,
    OFFSET,
    TAG
  };
}

namespace attribs_texgen {
  enum {
    COORDS,
    TAG
  };
}

struct Cut {
  Plane plane;
  Index tag;
};

void draw_cb() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glm::mat4 modelview = glm::rotate(
      glm::rotate(
        glm::scale(
          glm::translate(
            glm::mat4{},
            glm::vec3(0, 0, 3)),
          glm::vec3(1)),
        -0.3f, glm::vec3{1, 0, 0}),
      -globals::time, glm::vec3{0, 1, 0});
  glUseProgram(globals::prog_model);
  glBindVertexArray(globals::vao_model);
  glUniformMatrix4fv(uniforms_model::MODELVIEW, 1, GL_FALSE, glm::value_ptr(modelview));
  for(size_t i = 0; i < globals::counts.size(); i++) {
    glUniform1ui(uniforms_model::VOL_ID, i);
    glDrawElements(GL_TRIANGLES, globals::counts[i], GL_UNSIGNED_SHORT, globals::starts[i]);
  }
  glutSwapBuffers();
  globals::time += 0.01;
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

Volume init_shape(float size, const std::vector<Cut>& cuts) {
  Volume shape{size};
  for(const auto& cut : cuts)
    shape.cut(cut.plane, cut.tag);
  return shape;
}

void init_model(const Volume& shape, const std::vector<Plane>& cuts, const std::vector<glm::vec4>& colour_vals) {
  Mould m{shape};
  for(const auto& plane : cuts)
    m.cut(plane);
  
  std::vector<glm::vec3> coords{};
  std::vector<glm::vec3> normals{};
  std::vector<glm::vec4> colours{};
  std::vector<Index> indices{};

  globals::starts.resize(0);
  globals::counts.resize(0);

  for(auto& volume : m.get_volumes()) {
    ExtVolume ext(std::move(volume), 0.03);
    size_t base = coords.size();
    size_t indexBase = indices.size();
    const auto& vertices = ext.get_vertices();
    std::copy(begin(vertices), end(vertices), std::back_inserter(coords));
    for(const auto& face : ext.get_faces()) {
      std::fill_n(std::back_inserter(normals), face.indices.size(), face.normal);
      std::fill_n(std::back_inserter(colours), face.indices.size(), colour_vals[face.tag]);
    }
    append_face_list(indices, base, ext.get_faces());
    append_face_list(indices, base, ext.get_ext_faces());
    globals::starts.push_back(reinterpret_cast<void*>(indexBase * sizeof(Index)));
    globals::counts.push_back(indices.size() - indexBase);
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

  enum {
    COORDS_VBO,
    NORMALS_VBO,
    COLOURS_VBO,
    INDICES_IBO,
    MATRICES_SSBO
  };
  GLuint buffers[5];
  glGenBuffers(5, &buffers[0]);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[COORDS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, coords.size() * sizeof(coords[0]), coords.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs_model::COORDS);
  glVertexAttribPointer(attribs_model::COORDS, 3, GL_FLOAT, GL_FALSE, sizeof(coords[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[NORMALS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(normals[0]), normals.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs_model::NORMAL);
  glVertexAttribPointer(attribs_model::NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[COLOURS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, colours.size() * sizeof(colours[0]), colours.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs_model::COLOUR);
  glVertexAttribPointer(attribs_model::COLOUR, 4, GL_FLOAT, GL_FALSE, sizeof(colours[0]), nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[INDICES_IBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

  std::vector<glm::mat4> matrices(m.get_volumes().size());
  const auto& volumes = m.get_volumes();
  for(size_t i = 0; i < volumes.size(); i++)
    if(volumes[i].center() * cuts[0] > 0)
      matrices[i] = glm::rotate(glm::mat4{}, 0.5f, cuts[0].normal);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers[MATRICES_SSBO]);
  glBufferData(GL_SHADER_STORAGE_BUFFER, matrices.size() * sizeof(glm::mat4), glm::value_ptr(matrices[0]), GL_STATIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers[MATRICES_SSBO]);
}

void init_cubemap(unsigned texUnit, const Volume& main_volume, const std::vector<Cut>& shape_cuts, const std::vector<Plane>& cuts) {
  constexpr GLuint texSize = 512;

  struct {
    GLenum face;
    glm::mat4 proj;
  } faces[] = {
    {GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      {{0, 0, 0, 1},
       {0, 1, 0, 0},
       {-1, 0, 0, 0},
       {0, 0, 0, 0}}},
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      {{0, 0, 0, -1},
       {0, 1, 0, 0},
       {1, 0, 0, 0},
       {0, 0, 0, 0}}},
    {GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      {{1, 0, 0, 0},
       {0, 0, 0, 1},
       {0, -1, 0, 0},
       {0, 0, 0, 0}}},
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      {{1, 0, 0, 0},
       {0, 0, 0, -1},
       {0, 1, 0, 0},
       {0, 0, 0, 0}}},
    {GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      {{1, 0, 0, 0},
       {0, 1, 0, 0},
       {0, 0, 0, 1},
       {0, 0, 0, 0}}},
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      {{-1, 0, 0, 0},
       {0, 1, 0, 0},
       {0, 0, 0, -1},
       {0, 0, 0, 0}}}};

  GLuint framebuffer;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

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
  for(auto& [face, proj] : faces)
    glTexImage2D(face, 0, GL_RED, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

  GLutil::program prog_texgen{
    GLutil::shader{"texgen.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"texgen.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};

  // main volume faces

  std::vector<Index> indices{};
  std::vector<glm::uint> tags{};
  ExtVolume ext{main_volume};
  append_face_list(indices, 0, ext.get_faces());
  for(const auto& face : ext.get_faces())
    std::fill_n(std::back_inserter(tags), face.indices.size(), static_cast<glm::uint>(face.tag));

  GLuint vao_texgen;
  glGenVertexArrays(1, &vao_texgen);
  glBindVertexArray(vao_texgen);

  enum {
    COORDS_VBO,
    TAGS_VBO,
    INDICES_IBO
  };
  GLuint buffers[3];
  glGenBuffers(3, &buffers[0]);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[COORDS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, ext.get_vertices().size() * sizeof(Vertex), &ext.get_vertices()[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs_texgen::COORDS);
  glVertexAttribPointer(attribs_texgen::COORDS, 3, GL_FLOAT, GL_FALSE, sizeof(ext.get_vertices()[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[TAGS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, tags.size() * sizeof(tags[0]), &tags[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs_texgen::TAG);
  glVertexAttribIPointer(attribs_texgen::TAG, 1, GL_UNSIGNED_INT, sizeof(tags[0]), nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[INDICES_IBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(Index), &indices[0], GL_STATIC_DRAW);

  // paint cuts

  glViewport(0, 0, texSize, texSize);
  glClearColor(1, 1, 1, 1);
  glUseProgram(prog_texgen);
  glEnable(GL_BLEND);
  glBlendEquation(GL_MIN);

  for(auto& [face, proj] : faces) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face, texture, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniformMatrix4fv(uniforms_texgen::PROJ, 1, GL_FALSE, glm::value_ptr(proj));

    for(const auto& plane : cuts) {
      glUniform3fv(uniforms_texgen::NORMAL, 1, glm::value_ptr(plane.normal));
      glUniform1f(uniforms_texgen::OFFSET, plane.offset);
      glUniform1ui(uniforms_texgen::TAG, 0);
      glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
    }

    for(const auto& cut : shape_cuts) {
      glUniform3fv(uniforms_texgen::NORMAL, 1, glm::value_ptr(cut.plane.normal));
      glUniform1f(uniforms_texgen::OFFSET, cut.plane.offset);
      glUniform1ui(uniforms_texgen::TAG, cut.tag);
      glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
    }
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
    init_glut(argc, argv);
    GLutil::initGLEW();
    init_program();
    Volume shape = init_shape(2, shape_cuts);
    init_model(shape, cuts, colours);
    init_cubemap(tex_cubemap, shape, shape_cuts, cuts);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    glutMainLoop();
  } catch(const std::runtime_error& e) {
    std::cout.flush();
    std::cerr << e.what() << '\n';
  }
}
