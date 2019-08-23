#include "rubik.hpp"

namespace {
namespace model_attribs {
  enum {
    coords,
    normal,
    colour
  };
}

namespace texgen_attribs {
  enum {
    coords,
    normal,
    dummy,
    tag
  };
}

namespace click_outputs {
  enum {
    tag,
    coords,
    normal,
    count
  };
}
}


/***** COORDINATE TRANSFORMS *****/

glm::vec2 world_to_nd(const Context& ctx, glm::vec3 coords) {
  glm::vec4 tmp{ctx.mxs.proj * ctx.mxs.view * glm::vec4{coords, 1}};
  return glm::vec2{tmp} / tmp.w;
}

glm::vec3 model_to_world(const Context& ctx, glm::vec3 coords) {
  return glm::mat3{ctx.mxs.model} * coords;
}

glm::vec2 model_to_nd(const Context& ctx, glm::vec3 coords) {
  return world_to_nd(ctx, model_to_world(ctx, coords));
}

glm::vec4 nd_to_world(const Context& ctx, glm::vec2 coords_nd, float z) {
  glm::vec4 v{ctx.mxs.proj * ctx.mxs.view * glm::vec4{0, 0, z, 1}};
  glm::vec2 last_two = glm::vec2{v.z, v.w};
  return inverse(ctx.mxs.proj * ctx.mxs.view) * glm::vec4{last_two.y * coords_nd, last_two};
}

glm::vec2 window_to_nd(const Context& ctx, glm::ivec2 coords_win) {
  const auto w = ctx.gl.viewport.w,
             h = ctx.gl.viewport.h;
  return glm::vec2{(coords_win.x+0.5f)/w, -(coords_win.y+0.5f)/h} * 2.f - glm::vec2(1, -1);
}

glm::vec2 window_delta_to_nd(const Context& ctx, glm::vec2 delta_win) {
  const auto w = ctx.gl.viewport.w,
             h = ctx.gl.viewport.h;
  return glm::vec2{(float)delta_win.x/w, -(float)delta_win.y/h} * 2.f;
}

glm::vec4 window_delta_to_world(const Context& ctx, glm::vec2 delta_win, float z) {
  return nd_to_world(ctx, window_delta_to_nd(ctx, delta_win), z);
}

glm::vec2 nd_to_window_delta(const Context& ctx, glm::vec2 coords_nd) {
  const auto w = ctx.gl.viewport.w,
             h = ctx.gl.viewport.h;
  return glm::vec2{coords_nd.x * w / 2, -coords_nd.y * h / 2};
}

glm::vec2 model_to_window_delta(const Context& ctx, glm::vec3 coords) {
  return nd_to_window_delta(ctx, model_to_nd(ctx, coords));
}


/***** INITIALIZATION *****/

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

  ctx.gl.prog_click = GLutil::program{
    GLutil::shader{"click.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"click.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};
  ctx.gl.uniforms_click.matrix = glGetUniformLocation(ctx.gl.prog_click, "matrix");
  ctx.gl.uniforms_click.submodel = glGetUniformLocation(ctx.gl.prog_click, "submodel");
  ctx.gl.uniforms_click.volume = glGetUniformLocation(ctx.gl.prog_click, "volume");

  ctx.gl.prog_trivial = GLutil::program{
    GLutil::shader{"trivial.vert", GL_VERTEX_SHADER, GLutil::shader::from_file},
    GLutil::shader{"trivial.frag", GL_FRAGMENT_SHADER, GLutil::shader::from_file}};
}

Volume init_shape(Context&, float size, const std::vector<Plane>& shape_cuts) {
  Volume shape{size};
  size_t ix = 1;
  for(const auto& plane : shape_cuts)
    shape.cut(plane, ix++, Face::Type::outer);
  return shape;
}

void init_model(Context& ctx, const Volume& shape, const std::vector<Cut>& cuts, const std::vector<glm::vec4>& /*colour_vals*/) {
  Mould m{shape};
  for(const auto& cut : cuts)
    m.cut(cut.plane, cut.tag, Face::Type::inner);
  
  std::vector<glm::vec3> coords{};
  std::vector<glm::vec3> normals{};
  std::vector<glm::vec4> colours{};
  std::vector<Index> indices{};

  ctx.pieces.resize(0);
  for(auto volume : m.get_volumes()) {
    auto orig_volume = volume;
    volume.erode(0.03);
    volume.dilate(0.03);
    size_t base = coords.size();
    size_t indexBase = indices.size();
    const auto& vertices = volume.get_vertices();
    std::copy(begin(vertices), end(vertices), std::back_inserter(coords));
    for(const auto& face : volume.get_faces()) {
      if(face.type == Face::Type::bevel)
        continue;
      std::fill_n(std::back_inserter(normals), face.indices.size(), face.normal);
      std::fill_n(std::back_inserter(colours), face.indices.size(),
          glm::vec4(face.type == Face::Type::outer ? 1 : 0) /*colour_vals[face.tag]*/);
    }
    append_face_list(indices, base, volume.get_faces());
    ctx.pieces.push_back({
        orig_volume,
        glm::mat4{1},
        glm::mat4{1},
        reinterpret_cast<void*>(indexBase * sizeof(Index)),
        static_cast<GLint>(indices.size() - indexBase)});
  }

  glGenVertexArrays(1, &ctx.gl.vao_full);
  glBindVertexArray(ctx.gl.vao_full);

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
  glEnableVertexAttribArray(model_attribs::coords);
  glVertexAttribPointer(model_attribs::coords, 3, GL_FLOAT, GL_FALSE, sizeof(coords[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[NORMALS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(normals[0]), normals.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(model_attribs::normal);
  glVertexAttribPointer(model_attribs::normal, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[COLOURS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, colours.size() * sizeof(colours[0]), colours.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(model_attribs::colour);
  glVertexAttribPointer(model_attribs::colour, 4, GL_FLOAT, GL_FALSE, sizeof(colours[0]), nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[INDICES_IBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

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

void init_model_basic(Context& ctx, Volume shape) {
  std::vector<glm::vec3> normals{};
  std::vector<Index> indices{};
  std::vector<glm::uint> tags{};

  shape.dilate(0);
  for(const auto& face : shape.get_faces()) {
    if(face.type == Face::Type::bevel)
      continue;
    std::fill_n(std::back_inserter(tags), face.indices.size(), 
        static_cast<glm::uint>(face.type == Face::Type::outer ? face.tag : 0));
    std::fill_n(std::back_inserter(normals), face.indices.size(), face.normal);
  }
  append_face_list(indices, 0, shape.get_faces());

  glGenVertexArrays(1, &ctx.gl.vao_basic);
  glBindVertexArray(ctx.gl.vao_basic);

  enum {
    COORDS_VBO,
    NORMALS_VBO,
    TAGS_VBO,
    INDICES_IBO,
    BUFFER_COUNT
  };
  GLuint buffers[BUFFER_COUNT];
  glGenBuffers(BUFFER_COUNT, &buffers[0]);

  const auto& coords = shape.get_vertices();
  glBindBuffer(GL_ARRAY_BUFFER, buffers[COORDS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, coords.size() * sizeof(coords[0]), coords.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(model_attribs::coords);
  glVertexAttribPointer(model_attribs::coords, 3, GL_FLOAT, GL_FALSE, sizeof(coords[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[NORMALS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(normals[0]), normals.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(model_attribs::normal);
  glVertexAttribPointer(model_attribs::normal, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[TAGS_VBO]);
  glBufferData(GL_ARRAY_BUFFER, tags.size() * sizeof(tags[0]), &tags[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(texgen_attribs::tag);
  glVertexAttribIPointer(texgen_attribs::tag, 1, GL_UNSIGNED_INT, sizeof(tags[0]), nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[INDICES_IBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);
  ctx.gl.basic_size = indices.size();
}

void init_cubemap(Context& ctx, unsigned texUnit, const std::vector<Plane>& shape_cuts, const std::vector<Cut>& cuts) {
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

  // paint cuts

  glViewport(0, 0, texSize, texSize);
  glClearColor(1, 1, 1, 1);
  glUseProgram(prog_texgen);
  glEnable(GL_BLEND);
  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  glBindVertexArray(ctx.gl.vao_basic);

  for(auto& [face, proj] : faces) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face, texture, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniformMatrix4fv(uniforms.proj, 1, GL_FALSE, glm::value_ptr(proj));

    for(const auto& cut : cuts) {
      glUniform3fv(uniforms.p_normal, 1, glm::value_ptr(cut.plane.normal));
      glUniform1f(uniforms.p_offset, cut.plane.offset);
      glUniform1ui(uniforms.p_tag, 0);
      glDrawElements(GL_TRIANGLES, ctx.gl.basic_size, GL_UNSIGNED_SHORT, nullptr);
    }

    for(const auto& plane : shape_cuts) {
      glUniform3fv(uniforms.p_normal, 1, glm::value_ptr(plane.normal));
      glUniform1f(uniforms.p_offset, plane.offset);
      glUniform1ui(uniforms.p_tag, 1 + std::distance(&shape_cuts[0], &plane));
      glDrawElements(GL_TRIANGLES, ctx.gl.basic_size, GL_UNSIGNED_SHORT, nullptr);
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
    DEPTH,
    TAG,
    COORDS,
    NORMAL,
    BUFFER_COUNT
  };

  GLuint renderbuffers[BUFFER_COUNT];
  glGenRenderbuffers(BUFFER_COUNT, &renderbuffers[0]);

  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[DEPTH]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1, 1);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffers[DEPTH]);

  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[TAG]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_R32I, 1, 1);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + click_outputs::tag, GL_RENDERBUFFER, renderbuffers[TAG]);

  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[COORDS]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, 1, 1);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + click_outputs::coords, GL_RENDERBUFFER, renderbuffers[COORDS]);

  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[NORMAL]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, 1, 1);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + click_outputs::normal, GL_RENDERBUFFER, renderbuffers[NORMAL]);

  std::array<GLenum, click_outputs::count> buffers;
  std::iota(buffers.begin(), buffers.end(), GL_COLOR_ATTACHMENT0);
  glDrawBuffers(buffers.size(), &buffers[0]);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


/***** INTERNAL UI FUNCTIONS *****/

struct click_response {
  Index volume_index;
  glm::vec3 coords;
  glm::vec3 normal;
};

std::optional<click_response> window_to_model(const Context& ctx, glm::ivec2 coords_win) {
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.gl.fb_click);
  glViewport(-coords_win.x, -(ctx.gl.viewport.h-1-coords_win.y), ctx.gl.viewport.w, ctx.gl.viewport.h);
  glUseProgram(ctx.gl.prog_click);
  glUniformMatrix4fv(ctx.gl.uniforms_click.matrix, 1, GL_FALSE, glm::value_ptr(ctx.mxs.proj * ctx.mxs.view * ctx.mxs.model));

  glBindVertexArray(ctx.gl.vao_full);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  GLint index = 0;
  for(auto& piece : ctx.pieces) {
    glUniformMatrix4fv(ctx.gl.uniforms_click.submodel, 1, GL_FALSE, glm::value_ptr(piece.rotation));
    glUniform1i(ctx.gl.uniforms_click.volume, ++index);
    glDrawElements(GL_TRIANGLES, piece.gl_count, GL_UNSIGNED_SHORT, piece.gl_start);
  }
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glReadPixels(0, 0, 1, 1, GL_RED_INTEGER, GL_INT, &index);

  if(index == 0)
    return {};
  // else

  glBindVertexArray(ctx.gl.vao_basic);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUniformMatrix4fv(ctx.gl.uniforms_click.submodel, 1, GL_FALSE, glm::value_ptr(glm::mat4{1}));
  glDrawElements(GL_TRIANGLES, ctx.gl.basic_size, GL_UNSIGNED_SHORT, nullptr);
  glm::vec3 coords, normal;
  glReadBuffer(GL_COLOR_ATTACHMENT1);
  glReadPixels(0, 0, 1, 1, GL_RGB, GL_FLOAT, &coords);
  glReadBuffer(GL_COLOR_ATTACHMENT2);
  glReadPixels(0, 0, 1, 1, GL_RGB, GL_FLOAT, &normal);
  return {{static_cast<Index>(index - 1), coords, normal}};
}

void rotate_model(Context& ctx, glm::ivec2 coords_win, bool rewrite) {
  glm::vec2 offset_wld = glm::vec2{window_delta_to_world(ctx, glm::vec2{coords_win - ctx.ui.buttondown_win}, 0)};
  glm::mat4 model = glm::rotate(
      glm::rotate(
        glm::mat4{1},
        -offset_wld.x, {0, 1, 0}),
      offset_wld.y, {1, 0, 0}) * ctx.mxs.model;
  glUseProgram(ctx.gl.prog_model);
  glUniformMatrix4fv(ctx.gl.uniforms_model.modelview, 1, GL_FALSE, glm::value_ptr(ctx.mxs.view * model));
  if(rewrite)
    ctx.mxs.model = model;
}

void rotate_action(Context& ctx, glm::ivec2 coords_win) {
  glm::vec2 delta_win = glm::vec2{coords_win - ctx.ui.buttondown_win};
  using pair_type = decltype(ctx.ui.action_cuts)::value_type;
  auto best = *std::max_element(ctx.ui.action_cuts.begin(), ctx.ui.action_cuts.end(),
      [delta_win](const pair_type& a, const pair_type& b) {
        return std::abs(glm::dot(a.second, delta_win)) < std::abs(glm::dot(b.second, delta_win));
      });
  const Plane& p = best.first.plane;
  //float quality = glm::dot(glm::normalize(delta_win), best.second);
  float distance_win = glm::dot(best.second, delta_win);
  float distance_wld = (window_delta_to_world(ctx, glm::vec2{distance_win, 0}, 0)).x;
  /*float angle = std::abs(quality) > 0.7f
    ? distance_wld
    : 0.f;*/
  for(auto& piece : ctx.pieces) {
    piece.rotation_temp = piece.volume.center() < p
      ? glm::rotate(glm::mat4{1}, distance_wld, p.normal)
      : glm::mat4{1};
  }
}


/***** EXTERNAL UI FUNCTIONS *****/

void draw(const Context& ctx) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, ctx.gl.viewport.w, ctx.gl.viewport.h);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(ctx.gl.prog_model);
  glBindVertexArray(ctx.gl.vao_full);
  for(auto& piece : ctx.pieces) {
    glUniformMatrix4fv(ctx.gl.uniforms_model.submodel, 1, GL_FALSE, glm::value_ptr(piece.rotation * piece.rotation_temp));
    glDrawElements(GL_TRIANGLES, piece.gl_count, GL_UNSIGNED_SHORT, piece.gl_start);
  }
  if(ctx.ui.rot_action) {
    glUseProgram(ctx.gl.prog_trivial);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    std::vector<glm::vec2> coords{};
    coords.push_back(model_to_nd(ctx, ctx.ui.buttondown_mod));
    coords.push_back(model_to_nd(ctx, ctx.ui.buttondown_mod + ctx.ui.normal));
    for(const auto& a : ctx.ui.action_cuts) {
      coords.push_back(coords.front());
      coords.push_back(coords.front() + 10.f * window_delta_to_nd(ctx, a.second));
    }
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), coords.data());
    glEnableVertexAttribArray(0);
    glm::vec4 color = {0, 1, 0, 0.5};
    glUniform4fv(0, 1, glm::value_ptr(color));
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_LINES, 0, coords.size());
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
  }
}

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

void touch_start(Context& ctx, glm::ivec2 coords) {
  ctx.ui.buttondown_win = coords;
  if(auto response = window_to_model(ctx, coords); !response) {
    ctx.ui.rot_view = true;
  } else {
    const Piece& p = ctx.pieces[response->volume_index];
    const Volume& v = p.volume;
    auto cuts = v.get_rot_cuts();
    // Ignore rotations parallel to the touched surface and back-facing rotations
    cuts.erase(
        std::remove_if(cuts.begin(), cuts.end(), [&ctx, &p, n = response->normal](const Cut& c) {
          return std::abs(glm::dot(glm::mat3{p.rotation} * c.plane.normal, n)) > 0.99
            /*|| (glm::mat3{ctx.mxs.view * ctx.mxs.model * p.rotation} * c.plane.normal).z < -.5*/;
        }), cuts.end());
    if(!cuts.empty()) {
      ctx.ui.rot_action = true;
      ctx.ui.buttondown_mod = response->coords;
      ctx.ui.normal = response->normal;
      ctx.ui.action_center = v.center();
      ctx.ui.action_cuts = {};
      for(const auto& cut : cuts) {
        glm::vec2 normal_projected = glm::normalize(model_to_window_delta(ctx, cut.plane.normal));
        ctx.ui.action_cuts.push_back({cut, glm::vec2{normal_projected.y, -normal_projected.x}});
      }
    } else
      ctx.ui.rot_view = true;
  }
}

void touch_move(Context& ctx, glm::ivec2 coords) {
  if(ctx.ui.rot_view)
    rotate_model(ctx, coords, false);
  if(ctx.ui.rot_action)
    rotate_action(ctx, coords);
}

void touch_release(Context& ctx, glm::ivec2 coords) {
  if(ctx.ui.rot_view) {
    rotate_model(ctx, coords, true);
    ctx.ui.rot_view = false;
  } else if(ctx.ui.rot_action) {
    for(auto& piece : ctx.pieces)
      piece.rotation_temp = glm::mat4{1};
    ctx.ui.rot_action = false;
  }
}
