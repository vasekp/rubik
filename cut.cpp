//#define DEBUG
#include <iostream>
#include "Mould.hpp"
#include "GLutil.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace globals {
  GLuint vao_model;
  GLutil::program prog_model;
  GLuint all_matrices;
  GLuint this_matrix;
  std::vector<Vertex> centers;
  std::vector<GLvoid*> starts;
  std::vector<GLsizei> counts;
  std::vector<glm::mat4> matrices;
  size_t piece_count;
  float time = 0;
}

namespace uniforms_model {
  enum {
    MODEL,
    VIEW,
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

#ifdef DEBUG
void report(const std::string& r) {
  std::cout << std::hex;
  while(auto e = glGetError()) {
    std::cout << r << ": " << e << '\n';
  }
}
#endif

void draw(GLFWwindow* window) {
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

  Plane cut{{-1, 1, 1}, 0};
  for(size_t i = 0; i < globals::piece_count; i++)
    if(globals::centers[i] * cut > 0)
      globals::matrices[i] = glm::rotate(globals::matrices[i], 0.01f, cut.normal);

  glUseProgram(globals::prog_model);
  glBindVertexArray(globals::vao_model);
  glUniformMatrix4fv(uniforms_model::VIEW, 1, GL_FALSE, glm::value_ptr(modelview));
  for(size_t i = 0; i < globals::piece_count; i++) {
    glUniformMatrix4fv(uniforms_model::MODEL, 1, GL_FALSE, glm::value_ptr(globals::matrices[i]));
    glDrawElements(GL_TRIANGLES, globals::counts[i], GL_UNSIGNED_SHORT, globals::starts[i]);
  }

  glfwSwapBuffers(window);
  globals::time += 0.005;
}

void resize_cb(GLFWwindow* window, int w, int h) {
  if(w == 0 && h == 0)
    glfwGetFramebufferSize(window, &w, &h);
  glViewport(0, 0, w, h);
  float ratio = (float)w / h;
  glm::mat4 proj{
    {w > h ? 1/ratio : 1, 0, 0, 0},
    {0, w < h ? ratio : 1, 0, 0},
    {0, 0, .1, .2},
    {0, 0, 0, 1}};
  glProgramUniformMatrix4fv(globals::prog_model, uniforms_model::PROJ, 1, GL_FALSE, glm::value_ptr(proj));
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

void key_cb(GLFWwindow *window, unsigned key) {
  if(key == 'q')
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

GLFWwindow* init_glfw() {
  if(!glfwInit())
    throw std::runtime_error("glfwInit failed\n");;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  GLFWwindow* window = glfwCreateWindow(640, 480, "Title", NULL, NULL);
  if(!window)
    throw std::runtime_error("glfwCreateWindow failed");
  glfwSetCharCallback(window, &key_cb);
  glfwSetFramebufferSizeCallback(window, &resize_cb);
  glfwMakeContextCurrent(window);
  return window;
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
  globals::centers.resize(0);

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
    globals::centers.push_back(volume.center());
  }
  globals::piece_count = m.get_volumes().size();
  globals::matrices.resize(globals::piece_count);
  std::fill(begin(globals::matrices), end(globals::matrices), glm::mat4{});
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
    INDICES_IBO
  };
  GLuint buffers[4];
  glGenBuffers(4, &buffers[0]);

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
      draw(window);
      glfwPollEvents();
    }
  } catch(const std::runtime_error& e) {
    std::cout.flush();
    std::cerr << e.what() << '\n';
  }
  glfwTerminate();
}
