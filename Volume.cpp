#include "Mould.hpp"

#include <map>
#include <initializer_list>

Volume::Volume(float size) {
  for(float x : {-size, size})
  for(float y : {-size, size})
  for(float z : {-size, size})
    vertices.push_back({x, y, z});
  faces.push_back({{0, 2, 3, 1}, {-1, 0, 0}});
  faces.push_back({{4, 5, 7, 6}, {1, 0, 0}});
  faces.push_back({{0, 1, 5, 4}, {0, -1, 0}});
  faces.push_back({{2, 6, 7, 3}, {0, 1, 0}});
  faces.push_back({{0, 4, 6, 2}, {0, 0, -1}});
  faces.push_back({{1, 3, 7, 5}, {0, 0, 1}});
}

Vertex Volume::center() const {
  Vertex ret{};
  for(const auto& vx : vertices)
    ret += vx;
  return ret / float(vertices.size());
}

std::vector<Cut> Volume::get_rot_cuts() const {
  // Choose only faces that make a sensible rotation in Rubik models
  std::vector<Cut> ret{};
  Vertex c = center();
  for(const auto& f : faces) {
    float face_offset = glm::dot(f.normal, vertices[f.indices[0]]),
          center_offset = glm::dot(f.normal, c);
    if(face_offset < epsilon && center_offset < face_offset
        && glm::length(glm::cross(f.normal, c)) > epsilon)
      ret.push_back({{f.normal, face_offset}, f.tag});
  }
  // TODO: check on compatibility with other faces (for later)
  return ret;
}

#ifdef DEBUG
void Volume::dump() const {
  std::clog << "VOLUME:\n";
  for(Index ix = 0; ix < vertices.size(); ix++) {
    std::clog << ix << ": {"
      << vertices[ix].x << ", "
      << vertices[ix].y << ", "
      << vertices[ix].z << "}\n";
  }
  for(const auto& f : faces) {
    std::clog << "{ ";
    for(auto ix : f.indices)
      std::clog << ix << ' ';
    std::clog << "} n {"
      << f.normal.x << ", "
      << f.normal.y << ", "
      << f.normal.z << "}\n";
  }
}
#endif

Volume Volume::cut(const Plane& p, Index tag) {
#ifdef DEBUG
  std::clog << "\nVolume::cut\n";
#endif

  // Simple cases
  if(std::all_of(vertices.begin(), vertices.end(), [&p](const Vertex& vx) -> bool { return vx * p < epsilon; })) {
#ifdef DEBUG
    std::clog << "[keep all]\n";
#endif
    return {};
  } else if(std::all_of(vertices.begin(), vertices.end(), [&p](const Vertex& vx) -> bool { return vx * p > -epsilon; })) {
#ifdef DEBUG
    std::clog << "[drop all]\n";
#endif
    Volume ret{};
    std::swap(*this, ret);
    return ret;
  }

  add_intersections(p);

  Volume volIn{}, volOut{};
  std::vector<Index> section = find_section(p);
  assert(!section.empty());
#ifdef DEBUG
  std::clog << "Section: [ ";
  for(auto ix : section)
    std::clog << ix << ' ';
  std::clog << "]\n";
#endif
  std::vector<Index> secReverse = section;
  std::reverse(secReverse.begin(), secReverse.end());
  volIn.faces.emplace_back(std::move(section), p.normal, tag);
  volOut.faces.emplace_back(std::move(secReverse), -p.normal, tag);

  for(const auto& face : faces) {
    Face fIn{face.normal, face.tag}, fOut{face.normal, face.tag};
    for(auto ix : face.indices) {
      auto dot = vertices[ix] * p;
      if(dot < epsilon)
        fIn.indices.push_back(ix);
      if(dot > -epsilon)
        fOut.indices.push_back(ix);
    }
    if(fIn.indices.size() > 2)
      volIn.faces.push_back(std::move(fIn));
    if(fOut.indices.size() > 2)
      volOut.faces.push_back(std::move(fOut));
  }

  // take vertices from the original volume
  volIn.take_vertices_finalize(*this);
  volOut.take_vertices_finalize(*this);

#ifdef DEBUG
  volIn.dump();
  volOut.dump();
#endif

  std::swap(*this, volIn);
  return volOut;
}

void Volume::erode(float dist) {
  std::vector<Cut> cuts{};
  for(const auto& face : faces)
    cuts.push_back({{face.normal, glm::dot(face.normal, vertices[face.indices.front()]) - dist}, face.tag});
  for(const auto& cut : cuts)
    this->cut(cut.plane, cut.tag);
}

void Volume::dilate(float dist) {
  decltype(vertices) nvertices;
  decltype(faces) nfaces;
  Index ix_new{0};

  // Give each face its own set of vertices
  for(const auto& face : faces) {
    Face nface{face.normal, face.tag};
    for(auto ix : face.indices) {
      nvertices.push_back(vertices[ix]);
      nface.indices.push_back(ix_new++);
    }
    nfaces.push_back(std::move(nface));
  }

  // Now each faces[i] has its nfaces[i], which has the same size
  // and nfaces[i][j] is the i-th face's copy of vertex faces[i][j]

  // Make new (zero area) faces for edges
  for(const auto& face : faces) {
    auto i = &face - &faces[0];
    auto sz = face.indices.size();
    for(auto j = 0u; j < sz; j++) {
      Index orig1 = face[j], orig2 = face[j + 1];
      if(orig1 < orig2) {  // This condition guarantees that each edge is only counted once
        const Face& face2 = find_face(orig2, orig1);
        auto i2 = &face2 - &faces[0];
        auto j2 = face2.index(orig2);
        nfaces.push_back({{
            nfaces[i][j + 1], nfaces[i][j],
            nfaces[i2][j2 + 1], nfaces[i2][j2]},
          glm::normalize(face.normal + face2.normal), dilate_face_tag});
      }
    }
  }

  // Make new zero area faces for vertices
  std::vector<bool> seen(vertices.size());
  for(const auto& face : faces) {
    Index i = &face - &faces[0];
    auto sz = face.indices.size();
    for(auto j = 0u; j < sz; j++) {
      Index pivot = face.indices[j];
      if(seen[pivot])
        continue;
      glm::vec3 normal{};
      std::vector<Index> nface{nfaces[i][j]};
      for(Index ix = face[j + 1]; ; ) {
        const Face& f2 = find_face(ix, pivot);
        auto i2 = &f2 - &faces[0];
        auto j2 = f2.index(pivot);
        Index new_ix = nfaces[i2][j2];
        if(new_ix == nface.front())
          break;
        nface.push_back(new_ix);
        ix = f2[j2 + 1];
      }
      std::reverse(nface.begin(), nface.end());
      nfaces.emplace_back(std::move(nface), normal, dilate_face_tag);
      seen[pivot] = true;
    }
  }

  // Displace vertices
  for(const auto& nface : nfaces) {
    if(nface.tag == dilate_face_tag) // Ignore extra faces
      continue;
    glm::vec3 displ = dist * nface.normal;
    for(auto ix : nface.indices)
      nvertices[ix] += displ;
  }

  std::swap(vertices, nvertices);
  std::swap(faces, nfaces);
}

void Volume::add_intersections(const Plane& p) {
  for(auto& face : faces) {
    Index last_ix = face.indices.back();
    auto last_dot = vertices[last_ix] * p;
    for(auto i = 0u; i < face.indices.size(); i++) {
      Index cur_ix = face.indices[i];
      auto cur_dot = vertices[cur_ix] * p;
      if((cur_dot > epsilon && last_dot < -epsilon) || (cur_dot < -epsilon && last_dot > epsilon)) {
        Vertex new_vx = (cur_dot * vertices[last_ix] - last_dot * vertices[cur_ix]) / (cur_dot - last_dot);
        Index new_ix = static_cast<Index>(vertices.size());
        vertices.push_back(new_vx);
        Face& f2 = find_face(cur_ix, last_ix);
#ifdef DEBUG
        std::clog << "New vertex between " << last_ix << " and " << cur_ix << ": "
          << new_vx.x << ", " << new_vx.y << ", " << new_vx.z << " [" << new_ix << "]\n";
        std::clog << "Adding to " << face << " and " << f2 << '\n';
#endif
        face.indices.insert(face.indices.begin() + i, new_ix);
        f2.indices.insert(std::find(f2.indices.begin(), f2.indices.end(), last_ix), new_ix);
#ifdef DEBUG
        std::clog << "-> " << face << " and " << f2 << '\n';
#endif
      }
      std::tie(last_ix, last_dot) = std::tie(cur_ix, cur_dot);
    }
  }
}

std::vector<Index> Volume::find_section(const Plane& p) {
  for(const auto& face : faces) {
    unsigned cCross = 0;
    for(auto ix : face.indices) {
      auto dot = vertices[ix] * p;
      if(std::abs(dot) < epsilon)
        cCross++;
    }
    if(cCross >= 2)
      return traverse_start(face, p);
  }
  throw std::runtime_error("find_section() failed");
}

std::vector<Index> Volume::traverse_start(const Face& f, const Plane& p) {
  size_t i;
  auto sz = f.indices.size();
  for(i = 0; i < sz; i++)
    if(std::abs(vertices[f.indices[i]] * p) < epsilon)
      break;
  assert(i < sz);
  Index ixPivot = f[i];
  Index ixNeg = f[i + 1];
  Index ixPos = f[i - 1];
  if(vertices[ixNeg] * p > epsilon || vertices[ixPos] * p < -epsilon) {
    for(i++; i < sz; i++)
      if(std::abs(vertices[f.indices[i]] * p) < epsilon)
        break;
    assert(i < sz);
    ixPivot = f[i];
    ixNeg = f[i + 1];
    ixPos = f[i - 1];
  }
  assert(vertices[ixNeg] * p < epsilon && vertices[ixPos] * p > -epsilon);
  return traverse_nbours({ixPivot}, p, ixPivot, ixNeg, ixPos);
}

std::vector<Index> Volume::traverse_nbours(std::vector<Index> section, const Plane& p, Index ixPivot, Index ixNeg, Index ixPos) {
#ifdef DEBUG
  std::clog << "Traverse neighbours: "
    << "ixPivot = " << ixPivot
    << ", ixNeg = " << ixNeg
    << ", ixPos = " << ixPos << '\n';
#endif
  assert(std::abs(vertices[ixPivot] * p) < epsilon && vertices[ixNeg] * p < epsilon && vertices[ixPos] * p > -epsilon);
  Index ix = ixNeg;
  for(;;) {
#ifdef DEBUG
    std::clog << " ... " << ix << " (" << vertices[ix] * p << ")\n";
#endif
    if(std::abs(vertices[ix] * p) < epsilon) {
      if(section.front() == ix) // loop closed, done
        return section;
      else {
        section.push_back(ix);
        const Face& prev_face = find_face(ix, ixPivot);
        const Face& next_face = find_face(ixPivot, ix);
        Index new_pos = next_face.next(ix);
        Index new_neg = prev_face.prev(ix);
        return traverse_nbours(section, p, ix, new_neg, new_pos);
      }
    } else if(vertices[ix] * p > epsilon) {
      assert(ix != ixNeg);
      const Face& prev_face = find_face(ix, ixPivot);
      Index neg = prev_face.next(ixPivot);
      return traverse_face(section, p, prev_face, neg, ix);
    }
    if(ix == ixPos)
      throw std::runtime_error("traverse_nbours() failed!");
    const Face& next_face = find_face(ixPivot, ix);
    ix = next_face.prev(ixPivot);
  }
}

std::vector<Index> Volume::traverse_face(std::vector<Index> section, const Plane& p, const Face& face, Index ixNeg, Index ixPos) {
#ifdef DEBUG
  std::clog << "Traverse face: face = " << face
    << ", ixNeg = " << ixNeg
    << ", ixPos = " << ixPos << '\n';
#endif
  assert(vertices[ixNeg] * p < -epsilon && vertices[ixPos] * p > epsilon);
  for(auto i = face.index(ixNeg); face[i] != ixPos; i++) {
    Index ix = face[i];
#ifdef DEBUG
    std::clog << " ... " << ix << " (" << vertices[ix] * p << ")\n";
#endif
    if(std::abs(vertices[ix] * p) < epsilon) {
      if(section.front() == ix) // loop closed
        return section;
      else {
        section.push_back(ix);
        return traverse_nbours(section, p, ix, face[i - 1], face[i + 1]);
      }
    }
  }
  throw std::runtime_error("traverse_face() failed!");
}

Face& Volume::find_face(Index i1, Index i2) {
  for(auto& face : faces) {
    auto sz = face.indices.size();
    for(auto i = 0u; i < sz; i++)
      if(face[i] == i1 && face[i + 1] == i2)
        return face;
  }
  throw std::runtime_error("face not found");
}

void Volume::take_vertices_finalize(const Volume& orig) {
  Index newIx = 0;
  std::map<Index, Index> map{};
#ifdef DEBUG
  std::clog << "Remap: ";
#endif
  for(auto& f : faces)
    for(auto& ix : f.indices)
      if(auto it = map.find(ix); it != map.end())
        ix = it->second;
      else {
        vertices.push_back(orig.vertices[ix]);
#ifdef DEBUG
        std::clog << ix << "→" << newIx << ' ';
#endif
        ix = map[ix] = newIx++;
      }
#ifdef DEBUG
  std::clog << '\n';
#endif
}
