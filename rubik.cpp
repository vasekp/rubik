#include "rubik.hpp"

void update_proj(Context& ctx, int w, int h) {
  ctx.gl.viewport = {w, h};
  float ratio = (float)w / h;
  ctx.mxs.proj = glm::mat4{
    {w > h ? 1/ratio : 1, 0, 0, 0},
    {0, w < h ? ratio : 1, 0, 0},
    {0, 0, .1, .2},
    {0, 0, 0, 1}};
  glUseProgram(ctx.gl.prog_model);
  glUniformMatrix4fv(ctx.gl.uniforms_model.proj, 1, GL_FALSE, glm::value_ptr(ctx.mxs.proj));
}

void rotate_model(Context& ctx, glm::vec2 loc, bool rewrite) {
  loc -= ctx.ui.buttondown_loc;
  glm::vec2 last_two = [&ctx]() -> auto {
    glm::vec4 v{ctx.mxs.proj * glm::vec4{0, 0, 1, 1}};
    return glm::vec2{v.z, v.w};
  }();
  glm::vec2 modelcoord = glm::vec2{inverse(ctx.mxs.proj) * glm::vec4{last_two.y * loc, last_two}};
  glm::mat4 model = glm::rotate(
      glm::rotate(
        glm::mat4{1},
        -modelcoord.x, {0, 1, 0}),
      modelcoord.y, {1, 0, 0}) * ctx.mxs.model;
  glUseProgram(ctx.gl.prog_model);
  glUniformMatrix4fv(ctx.gl.uniforms_model.modelview, 1, GL_FALSE, glm::value_ptr(ctx.mxs.view * model));
  if(rewrite)
    ctx.mxs.model = model;
}

void draw(Context& ctx, int t) {
  glViewport(0, 0, ctx.gl.viewport.w, ctx.gl.viewport.h);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(ctx.gl.prog_model);
  glBindVertexArray(ctx.gl.vao_model);
  int tag = 0;
  for(auto& piece : ctx.pieces) {
    glUniformMatrix4fv(ctx.gl.uniforms_model.submodel, 1, GL_FALSE, glm::value_ptr(piece.rotation));
    glUniform1i(ctx.gl.uniforms_model.highlight, ++tag == t ? GL_TRUE : GL_FALSE);
    glDrawElements(GL_TRIANGLES, piece.gl_count, GL_UNSIGNED_SHORT, piece.gl_start);
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

void init_programs(Context& ctx) {
  ctx.gl.prog_model = GLutil::program{
    GLutil::shader{"model.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"model.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};
  ctx.gl.uniforms_model.submodel = glGetUniformLocation(ctx.gl.prog_model, "submodel");
  ctx.gl.uniforms_model.modelview = glGetUniformLocation(ctx.gl.prog_model, "modelview");
  ctx.gl.uniforms_model.proj = glGetUniformLocation(ctx.gl.prog_model, "proj");
  ctx.gl.uniforms_model.texture = glGetUniformLocation(ctx.gl.prog_model, "sampler");
  ctx.gl.uniforms_model.highlight = glGetUniformLocation(ctx.gl.prog_model, "highlight");

  ctx.gl.prog_click = GLutil::program{
    GLutil::shader{"click.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"click.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};
  ctx.gl.uniforms_click.matrix = glGetUniformLocation(ctx.gl.prog_click, "matrix");
  ctx.gl.uniforms_click.submodel = glGetUniformLocation(ctx.gl.prog_click, "submodel");
  ctx.gl.uniforms_click.location = glGetUniformLocation(ctx.gl.prog_click, "loc");
  ctx.gl.uniforms_click.tag = glGetUniformLocation(ctx.gl.prog_click, "u_tag");
}

Volume init_shape(Context&, float size, const std::vector<Cut>& cuts) {
  Volume shape{size};
  for(const auto& cut : cuts)
    shape.cut(cut.plane, cut.tag);
  return shape;
}

void init_model(Context& ctx, const Volume& shape, const std::vector<Plane>& cuts, const std::vector<glm::vec4>& /*colour_vals*/) {
  Mould m{shape};
  for(const auto& plane : cuts)
    m.cut(plane);
  
  std::vector<glm::vec3> coords{};
  std::vector<glm::vec3> normals{};
  std::vector<glm::vec4> colours{};
  std::vector<Index> indices{};

  ctx.pieces.resize(0);
  for(auto volume : m.get_volumes()) {
    volume.erode(0.03);
    volume.dilate(0.03);
    size_t base = coords.size();
    size_t indexBase = indices.size();
    const auto& vertices = volume.get_vertices();
    std::copy(begin(vertices), end(vertices), std::back_inserter(coords));
    for(const auto& face : volume.get_faces()) {
      if(face.tag == Volume::dilate_face_tag)
        continue;
      std::fill_n(std::back_inserter(normals), face.indices.size(), face.normal);
      std::fill_n(std::back_inserter(colours), face.indices.size(), glm::vec4(face.tag > 0 ? 1 : 0) /*colour_vals[face.tag]*/);
    }
    append_face_list(indices, base, volume.get_faces());
    ctx.pieces.push_back({
        volume,
        volume.center(),
        glm::mat4{1},
        reinterpret_cast<void*>(indexBase * sizeof(Index)),
        static_cast<GLint>(indices.size() - indexBase)});
  }

  struct {
    GLint coords;
    GLint normal;
    GLint colour;
  } attribs = {
    glGetAttribLocation(ctx.gl.prog_model, "in_coords"),
    glGetAttribLocation(ctx.gl.prog_model, "in_normal"),
    glGetAttribLocation(ctx.gl.prog_model, "in_colour")
  };

  glGenVertexArrays(1, &ctx.gl.vao_model);
  glBindVertexArray(ctx.gl.vao_model);

  enum {
    COORDS_VBO,
    NORMALS_VBO,
    COLOURS_VBO,
    INDICES_IBO,
    BUFFER_COUNT
  };
  GLuint buffers[BUFFER_COUNT];
  glGenBuffers(BUFFER_COUNT, &buffers[0]);

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

  // a simplified VAO for click event processing

  glGenVertexArrays(1, &ctx.gl.vao_click);
  glBindVertexArray(ctx.gl.vao_click);

  GLint click_coords = glGetAttribLocation(ctx.gl.prog_click, "coords");
  glBindBuffer(GL_ARRAY_BUFFER, buffers[COORDS_VBO]);
  glEnableVertexAttribArray(click_coords);
  glVertexAttribPointer(click_coords, 3, GL_FLOAT, GL_FALSE, sizeof(coords[0]), nullptr);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[INDICES_IBO]);

  ctx.mxs.view = glm::translate(glm::mat4{1}, glm::vec3(0, 0, 3));
  ctx.mxs.model = glm::rotate(
      glm::rotate(
        glm::scale(
          glm::mat4{1},
          glm::vec3{1.5f}),
        -0.3f, glm::vec3{1, 0, 0}),
      0.2f, glm::vec3{0, 1, 0});
  glUseProgram(ctx.gl.prog_model);
  glUniformMatrix4fv(ctx.gl.uniforms_model.modelview, 1, GL_FALSE, glm::value_ptr(ctx.mxs.view * ctx.mxs.model));
}

void init_cubemap(Context& ctx, unsigned texUnit, const Volume& main_volume, const std::vector<Cut>& shape_cuts, const std::vector<Plane>& cuts) {
  constexpr GLuint texSize = 1024;

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
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  for(auto& [face, proj] : faces)
    glTexImage2D(face, 0, GL_R32F, texSize, texSize, 0, GL_RED, GL_FLOAT, nullptr);

  GLutil::program prog_texgen{
    GLutil::shader{"texgen.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"texgen.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};

  struct {
    GLint proj;
    GLint p_normal;
    GLint p_offset;
    GLint p_tag;
  } uniforms = {
    glGetUniformLocation(prog_texgen, "proj"),
    glGetUniformLocation(prog_texgen, "p_normal"),
    glGetUniformLocation(prog_texgen, "p_offset"),
    glGetUniformLocation(prog_texgen, "p_tag")
  };
  struct {
    GLint coords;
    GLint tag;
    GLint normal;
  } attribs = {
    glGetAttribLocation(prog_texgen, "in_coords"),
    glGetAttribLocation(prog_texgen, "in_tag"),
    glGetAttribLocation(prog_texgen, "in_normal")
  };

  // main volume faces

  std::vector<Index> indices{};
  std::vector<glm::uint> tags{};
  std::vector<glm::vec3> normals{};
  Volume ext = main_volume;
  ext.dilate(0);
  append_face_list(indices, 0, ext.get_faces());
  for(const auto& face : ext.get_faces()) {
    if(face.tag == Volume::dilate_face_tag)
      continue;
    std::fill_n(std::back_inserter(tags), face.indices.size(), static_cast<glm::uint>(face.tag));
    std::fill_n(std::back_inserter(normals), face.indices.size(), face.normal);
  }

  GLuint vao_texgen;
  glGenVertexArrays(1, &vao_texgen);
  glBindVertexArray(vao_texgen);

  enum {
    COORDS_VBO,
    TAGS_VBO,
    NORMALS_VBO,
    INDICES_IBO,
    BUFFER_COUNT
  };
  GLuint buffers[BUFFER_COUNT];
  glGenBuffers(BUFFER_COUNT, &buffers[0]);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[COORDS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, ext.get_vertices().size() * sizeof(Vertex), &ext.get_vertices()[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs.coords);
  glVertexAttribPointer(attribs.coords, 3, GL_FLOAT, GL_FALSE, sizeof(ext.get_vertices()[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[TAGS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, tags.size() * sizeof(tags[0]), &tags[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs.tag);
  glVertexAttribIPointer(attribs.tag, 1, GL_UNSIGNED_INT, sizeof(tags[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[NORMALS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(attribs.normal);
  glVertexAttribPointer(attribs.normal, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[INDICES_IBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(Index), &indices[0], GL_STATIC_DRAW);

  // paint cuts

  glViewport(0, 0, texSize, texSize);
  glClearColor(1, 1, 1, 1);
  glUseProgram(prog_texgen);
  glEnable(GL_BLEND);
  glBlendFunc(GL_DST_COLOR, GL_ZERO);

  for(auto& [face, proj] : faces) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face, texture, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniformMatrix4fv(uniforms.proj, 1, GL_FALSE, glm::value_ptr(proj));

    for(const auto& plane : cuts) {
      glUniform3fv(uniforms.p_normal, 1, glm::value_ptr(plane.normal));
      glUniform1f(uniforms.p_offset, plane.offset);
      glUniform1ui(uniforms.p_tag, 0);
      glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
    }

    for(const auto& cut : shape_cuts) {
      glUniform3fv(uniforms.p_normal, 1, glm::value_ptr(cut.plane.normal));
      glUniform1f(uniforms.p_offset, cut.plane.offset);
      glUniform1ui(uniforms.p_tag, cut.tag);
      glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
    }
  }

  glUseProgram(ctx.gl.prog_model);
  glUniform1i(ctx.gl.uniforms_model.texture, texUnit);
  glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST);

  // reset to sensible state

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_BLEND);
  glClearColor(0, 0, 0, 1);
}

void init_click_target(Context& ctx) {
  glGenFramebuffers(1, &ctx.gl.fb_click);
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.gl.fb_click);

  enum {
    TAG,
    DEPTH,
    BUFFER_COUNT
  };

  GLuint renderbuffers[BUFFER_COUNT];
  glGenRenderbuffers(BUFFER_COUNT, &renderbuffers[0]);

  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[TAG]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_R32I, 1, 1);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffers[TAG]);

  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[DEPTH]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1, 1);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffers[DEPTH]);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLint get_click_volume(Context& ctx, glm::vec2 point) {
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.gl.fb_click);
  glViewport(0, 0, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(ctx.gl.prog_click);
  glBindVertexArray(ctx.gl.vao_click);
  glUniformMatrix4fv(ctx.gl.uniforms_click.matrix, 1, GL_FALSE, glm::value_ptr(ctx.mxs.proj * ctx.mxs.view * ctx.mxs.model));
  glUniform2fv(ctx.gl.uniforms_click.location, 1, glm::value_ptr(point));
  glBindVertexArray(ctx.gl.vao_model);
  GLint tag = 0;
  for(auto& piece : ctx.pieces) {
    glUniformMatrix4fv(ctx.gl.uniforms_click.submodel, 1, GL_FALSE, glm::value_ptr(piece.rotation));
    glUniform1i(ctx.gl.uniforms_click.tag, ++tag);
    glDrawElements(GL_TRIANGLES, piece.gl_count, GL_UNSIGNED_SHORT, piece.gl_start);
  }
  glReadPixels(0, 0, 1, 1, GL_RED_INTEGER, GL_INT, &tag);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return tag;
}
