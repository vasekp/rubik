//#define DEBUG
#include "rubik.hpp"

void update_proj(Context& ctx, int w, int h) {
  glViewport(0, 0, w, h);
  float ratio = (float)w / h;
  ctx.proj = glm::mat4{
    {w > h ? 1/ratio : 1, 0, 0, 0},
    {0, w < h ? ratio : 1, 0, 0},
    {0, 0, .1, .2},
    {0, 0, 0, 1}};
  glUseProgram(ctx.prog_model);
  glUniformMatrix4fv(ctx.uniforms_model.proj, 1, GL_FALSE, glm::value_ptr(ctx.proj));
  ctx.window_size = glm::vec2{w, h};
}

void rotate_model(Context& ctx, glm::vec2 loc, bool rewrite) {
  loc -= ctx.buttondown_loc;
  loc /= ctx.window_size/2.f;
  glm::vec2 last_two = [&ctx]() -> auto {
    glm::vec4 v{ctx.proj * glm::vec4{0, 0, 1, 1}};
    return glm::vec2{v.z, v.w};
  }();
  glm::vec2 modelcoord = glm::vec2{inverse(ctx.proj) * glm::vec4{loc, last_two}};
  glm::mat4 model = glm::rotate(
      glm::rotate(
        glm::mat4{},
        -modelcoord.x, {0, 1, 0}),
      -modelcoord.y, {1, 0, 0}) * ctx.model;
  glUseProgram(ctx.prog_model);
  glUniformMatrix4fv(ctx.uniforms_model.modelview, 1, GL_FALSE, glm::value_ptr(ctx.view * model));
  if(rewrite)
    ctx.model = model;
}

void draw(Context& ctx) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  Plane cut{{-1, 1, 1}, 0};
  for(size_t i = 0; i < ctx.piece_count; i++)
    if(ctx.centers[i] * cut > 0)
      ctx.matrices[i] = glm::rotate(ctx.matrices[i], 0.01f, cut.normal);

  glUseProgram(ctx.prog_model);
  glBindVertexArray(ctx.vao_model);
  for(size_t i = 0; i < ctx.piece_count; i++) {
    glUniformMatrix4fv(ctx.uniforms_model.submodel, 1, GL_FALSE, glm::value_ptr(ctx.matrices[i]));
    glDrawElements(GL_TRIANGLES, ctx.counts[i], GL_UNSIGNED_SHORT, ctx.starts[i]);
  }
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

void init_program(Context& ctx) {
  ctx.prog_model = GLutil::program{
    GLutil::shader{"model.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"model.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};
  ctx.uniforms_model.submodel = glGetUniformLocation(ctx.prog_model, "submodel");
  ctx.uniforms_model.modelview = glGetUniformLocation(ctx.prog_model, "modelview");
  ctx.uniforms_model.proj = glGetUniformLocation(ctx.prog_model, "proj");
  ctx.uniforms_model.texture = glGetUniformLocation(ctx.prog_model, "sampler");
}

Volume init_shape(Context&, float size, const std::vector<Cut>& cuts) {
  Volume shape{size};
  for(const auto& cut : cuts)
    shape.cut(cut.plane, cut.tag);
  return shape;
}

void init_model(Context& ctx, const Volume& shape, const std::vector<Plane>& cuts, const std::vector<glm::vec4>& colour_vals) {
  Mould m{shape};
  for(const auto& plane : cuts)
    m.cut(plane);
  
  std::vector<glm::vec3> coords{};
  std::vector<glm::vec3> normals{};
  std::vector<glm::vec4> colours{};
  std::vector<Index> indices{};

  ctx.starts.resize(0);
  ctx.counts.resize(0);
  ctx.centers.resize(0);

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
    ctx.starts.push_back(reinterpret_cast<void*>(indexBase * sizeof(Index)));
    ctx.counts.push_back(indices.size() - indexBase);
    ctx.centers.push_back(volume.center());
  }
  ctx.piece_count = m.get_volumes().size();
  ctx.matrices.resize(ctx.piece_count);
  std::fill(begin(ctx.matrices), end(ctx.matrices), glm::mat4{});

  struct {
    GLint coords;
    GLint normal;
    GLint colour;
  } attribs = {
    glGetAttribLocation(ctx.prog_model, "in_coords"),
    glGetAttribLocation(ctx.prog_model, "in_normal"),
    glGetAttribLocation(ctx.prog_model, "in_colour")
  };

  glGenVertexArrays(1, &ctx.vao_model);
  glBindVertexArray(ctx.vao_model);

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
  glEnableVertexAttribArray(attribs.coords);
  glVertexAttribPointer(attribs.coords, 3, GL_FLOAT, GL_FALSE, sizeof(coords[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[NORMALS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(normals[0]), normals.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs.normal);
  glVertexAttribPointer(attribs.normal, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[COLOURS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, colours.size() * sizeof(colours[0]), colours.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs.colour);
  glVertexAttribPointer(attribs.colour, 4, GL_FLOAT, GL_FALSE, sizeof(colours[0]), nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[INDICES_IBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

  ctx.view = glm::translate(glm::mat4{}, glm::vec3(0, 0, 3));
  ctx.model = glm::rotate(
      glm::rotate(
        glm::mat4{},
        -0.3f, glm::vec3{1, 0, 0}),
      -1.f, glm::vec3{0, 1, 0});
  glUseProgram(ctx.prog_model);
  glUniformMatrix4fv(ctx.uniforms_model.modelview, 1, GL_FALSE, glm::value_ptr(ctx.view * ctx.model));
}

void init_cubemap(Context& ctx, unsigned texUnit, const Volume& main_volume, const std::vector<Cut>& shape_cuts, const std::vector<Plane>& cuts) {
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
  for(auto& [face, proj] : faces)
    glTexImage2D(face, 0, GL_RGB, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

  GLutil::program prog_texgen{
    GLutil::shader{"texgen.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"texgen.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};

  struct {
    GLint proj;
    GLint normal;
    GLint offset;
    GLint tag;
  } uniforms = {
    glGetUniformLocation(prog_texgen, "proj"),
    glGetUniformLocation(prog_texgen, "normal"),
    glGetUniformLocation(prog_texgen, "off"),
    glGetUniformLocation(prog_texgen, "u_tag")
  };
  struct {
    GLint coords;
    GLint tag;
  } attribs = {
    glGetAttribLocation(prog_texgen, "in_coords"),
    glGetAttribLocation(prog_texgen, "in_tag")
  };

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
  glEnableVertexAttribArray(attribs.coords);
  glVertexAttribPointer(attribs.coords, 3, GL_FLOAT, GL_FALSE, sizeof(ext.get_vertices()[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[TAGS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, tags.size() * sizeof(tags[0]), &tags[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs.tag);
  glVertexAttribIPointer(attribs.tag, 1, GL_UNSIGNED_INT, sizeof(tags[0]), nullptr);

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
    glUniformMatrix4fv(uniforms.proj, 1, GL_FALSE, glm::value_ptr(proj));

    for(const auto& plane : cuts) {
      glUniform3fv(uniforms.normal, 1, glm::value_ptr(plane.normal));
      glUniform1f(uniforms.offset, plane.offset);
      glUniform1ui(uniforms.tag, 0);
      glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
    }

    for(const auto& cut : shape_cuts) {
      glUniform3fv(uniforms.normal, 1, glm::value_ptr(cut.plane.normal));
      glUniform1f(uniforms.offset, cut.plane.offset);
      glUniform1ui(uniforms.tag, cut.tag);
      glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
    }
  }

  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
  glUseProgram(ctx.prog_model);
  glUniform1i(ctx.uniforms_model.texture, texUnit);

  // reset to sensible state

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBlendEquation(GL_FUNC_ADD);
  glDisable(GL_BLEND);
  glClearColor(0, 0, 0, 1);
}
