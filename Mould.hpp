#include <cstddef> // size_t
#include <cstdint> // uint16_t
#include <iterator>
#include <vector>
#include <map>
#include <algorithm>
#include <glm/glm.hpp>

#ifdef DEBUG
#include <iostream>
#endif

using std::size_t;
using Index = uint16_t;
using Vertex = glm::vec3;

struct Plane {
  glm::vec3 normal;
  float offset;

  friend float operator*(const Vertex& v, const Plane& p) {
    return dot(p.normal, v) - p.offset;
  }
};

struct Face {
  std::vector<Index> vertices;
  glm::vec3 normal;
};

class Mould {
  std::vector<Vertex> vertices;
  std::vector<Face> faces;

public:

  Mould(float size = 2) {
    for(float x = -1; x <= 1; x += 2)
    for(float y = -1; y <= 1; y += 2)
    for(float z = -1; z <= 1; z += 2)
      vertices.push_back({size*x, size*y, size*z});
    faces.push_back({{0, 2, 3, 1}, {-1, 0, 0}});
    faces.push_back({{4, 5, 7, 6}, {1, 0, 0}});
    faces.push_back({{0, 1, 5, 4}, {0, -1, 0}});
    faces.push_back({{2, 6, 7, 3}, {0, 1, 0}});
    faces.push_back({{0, 4, 6, 2}, {0, 0, 1}});
    faces.push_back({{1, 3, 7, 5}, {0, 0, -1}});
  }

  void cut(Plane p, bool discard = false) {
    std::vector<Face> nfaces{};
    Face nIn{}, nOut{};
    nIn.normal = p.normal;
    nOut.normal = -p.normal;
    for(auto& f : faces) {
      unsigned cIn = 0, cOut = 0;
      for(auto ix : f.vertices)
        if(Vertex vx = vertices[ix]; vx * p < 0)
          cIn++;
        else if(vx * p > 0)
          cOut++;
#ifdef DEBUG
      std::cout << "- " << cIn << " + " << cOut << '\n';
#endif

      if(cOut == 0)
        keep_face(nfaces, std::move(f));
      if(cIn == 0) {
        if(!discard)
          keep_face(nfaces, std::move(f));
        else
          drop_face(f);
      }
      if(cIn == 0 || cOut == 0)
        continue;

      Face fIn{}, fOut{};
      fIn.normal = f.normal;
      fOut.normal = f.normal;
      const Vertex *last = &vertices[f.vertices.back()];
      float ldot = *last * p;
      for(auto ix : f.vertices) {
        const Vertex *cur = &vertices[ix];
        float dot = *cur * p;
        if(ldot * dot < 0) {
          auto intersect = (dot * *last - ldot * *cur)/(dot - ldot);
          auto [newIx, added] = find_or_append(std::move(intersect));
          fIn.vertices.push_back(newIx);
          nIn.vertices.push_back(newIx);
          if(!discard) {
            fOut.vertices.push_back(newIx);
            nOut.vertices.push_back(newIx);
          }
        }
        else if(dot == 0) {
          fIn.vertices.push_back(ix);
          nIn.vertices.push_back(ix);
          if(!discard) {
            fOut.vertices.push_back(ix);
            nOut.vertices.push_back(ix);
          }
        }
        if(dot < 0)
          fIn.vertices.push_back(ix);
        else if(dot > 0 && !discard)
          fOut.vertices.push_back(ix);
        last = cur;
        ldot = dot;
      }
      drop_face(std::move(f));
      add_face(nfaces, std::move(fIn));
      if(!discard)
        add_face(nfaces, std::move(fOut));
    }
    add_face(nfaces, sort_ccw(nIn));
    if(!discard)
      add_face(nfaces, sort_ccw(nOut));
    swap(faces, nfaces);
    cleanup();
  }

  void clip_edges(float dist = 0.1) {
    std::vector<Vertex> nvertices{};
    std::vector<Face> ext_faces{};
    std::vector<Face> ext_edges{};
    std::vector<Face> ext_vertices{};

    size_t nvcount = 0;
    for(const auto& f : faces)
      nvcount += f.vertices.size();
    nvertices.reserve(nvcount);
    ext_faces.reserve(faces.size());
    ext_edges.reserve(nvcount);
    ext_vertices.resize(vertices.size());

    // copy faces, blow up vertices
    // each face gets a unique set of new vertices
    // new vertex indices predictable
    for(const auto& f : faces) {
      size_t base = nvertices.size();
      //size_t size = f.vertices.size();
      size_t newIx = base;
      std::vector<Index> newIxs{};
      for(auto ix : f.vertices) {
        nvertices.push_back(vertices[ix]); // new element @ newIx
        ext_vertices[ix].vertices.push_back(newIx);
        ext_vertices[ix].normal += f.normal;
        newIxs.push_back(newIx++);
      }
      ext_faces.push_back({newIxs, f.normal});
    }

    // find coincident edges, make edge faces
    for(size_t fc = faces.size(), fi1 = 0; fi1 < fc; fi1++) {
      const auto& f1 = faces[fi1];
      size_t vc1 = f1.vertices.size();
      for(size_t vi1 = 0; vi1 < vc1; vi1++) {
        size_t vj1 = (vi1 + 1) % vc1;
        Index vix1 = f1.vertices[vi1],
              vjx1 = f1.vertices[vj1];
        bool found = false;
        // [find vi1->vj1 edge in another face]
        for(size_t fi2 = fi1 + 1; !found && fi2 < fc; fi2++) {
          const auto& f2 = faces[fi2];
          size_t vc2 = f2.vertices.size();
          for(size_t vi2 = 0; vi2 < vc2; vi2++) {
            size_t vj2 = (vi2 + 1) % vc2;
            Index vix2 = f2.vertices[vi2],
                  vjx2 = f2.vertices[vj2];
            // must be oriented opposite
            if(vix1 == vjx2 && vix2 == vjx1) {
              // edge found!
              found = true;
#ifdef DEBUG
              std::cout << 'f' << fi1 << " i" << vi1 << " j" << vj1 <<
                " = f" << fi2 << " i" << vi2 << " j" << vj2 << '\n';
#endif
              // vi1, vj1 indices within f1 = faces[fi1]
              // vi2, vj2 indices within f2 = faces[fi2]
              // we need to reindex to nvertices, i.e., take vector indices from ext_faces
              std::vector<Index> edge_ixs = {
                ext_faces[fi1].vertices[vj1],
                ext_faces[fi1].vertices[vi1],
                ext_faces[fi2].vertices[vj2],
                ext_faces[fi2].vertices[vi2]
              };
              ext_edges.push_back({edge_ixs, f1.normal + f2.normal});
              break;
            } // found
          } // vi2
        } // fi2
      } // vi1
    } // fi1

    // shrink all new faces (affecting nvertices referenced from all structures)
    for(const auto& f : ext_faces) {
      auto& normal = f.normal;
      size_t size = f.vertices.size();
      auto* vprev = &nvertices[f.vertices.back()];
      auto* vthis = &nvertices[f.vertices.front()];
      std::vector<Vertex> shrunk(size);
      for(size_t i = 0; i < size; i++) {
        auto* vnext = &nvertices[f.vertices[(i + 1) % size]];
        auto dprev = normalize(*vthis - *vprev);
        auto dnext = normalize(*vnext - *vthis);
        auto cross = glm::cross(dprev, normal);
        auto triple = dot(dnext, cross);
#ifdef DEBUG
        std::cout << "Vertex " << f.vertices[i] << ": "
          << "|dprev| = " << length(dprev)
          << ", |dnext| = " << length(dnext)
          << ", |cross| = " << length(cross)
          << ", triple = " << triple << '\n';
#endif
        shrunk[i] = *vthis + dist * cross - triple * dist / (1 + dot(dprev, dnext)) * dprev;
        vprev = vthis;
        vthis = vnext;
      }
      // safe to overwrite now
      for(size_t i = 0; i < size; i++) {
#ifdef DEBUG
        std::cout << "Vertex " << f.vertices[i] << " shrunk by "
          << distance(nvertices[f.vertices[i]], shrunk[i]) << '\n';
#endif
        nvertices[f.vertices[i]] = shrunk[i];
      }
    }

    // fix all new faces
    swap(vertices, nvertices);
    for(auto& f : ext_edges)
      f.normal = normalize(f.normal);
      // ext edge faces already CCW by construction
    for(auto& f : ext_vertices) {
      f.normal = normalize(f.normal);
      f = sort_ccw(f);
    }

    faces.resize(0);
    faces.reserve(ext_faces.size() + ext_edges.size() + ext_vertices.size());
    using MoveIter = std::move_iterator<decltype(begin(faces))>;
    std::copy(MoveIter(begin(ext_faces)), MoveIter(end(ext_faces)), std::back_inserter(faces));
    std::copy(MoveIter(begin(ext_vertices)), MoveIter(end(ext_vertices)), std::back_inserter(faces));
    std::copy(MoveIter(begin(ext_edges)), MoveIter(end(ext_edges)), std::back_inserter(faces));
    dump();
  }

private:
  constexpr static float epsilon = 0.01;

  void add_face(std::vector<Face>& nfaces, Face&& face) {
#ifdef DEBUG
    std::cout << "adding { ";
    for(auto ix : face.vertices)
      std::cout << ix << ' ';
    std::cout << "}\n";
#endif
    nfaces.push_back(std::move(face));
  }

  // no-op (we just don't add it), but useful for debugging
#ifdef DEBUG
  void drop_face(const Face& face) {
    std::cout << "dropping { ";
    for(auto ix : face.vertices)
      std::cout << ix << ' ';
    std::cout << "}\n";
  }
#else
  void drop_face(const Face&) { }
#endif

  void keep_face(std::vector<Face>& nfaces, Face&& face) {
#ifdef DEBUG
    std::cout << "keeping { ";
    for(auto ix : face.vertices)
      std::cout << ix << ' ';
    std::cout << "}\n";
#endif
    nfaces.push_back(std::move(face));
  }

  std::pair<Index, bool> find_or_append(Vertex&& vx) {
    for(const auto& vy : vertices)
      if(distance(vx, vy) < epsilon)
        return {&vy - &vertices[0], false};
    // else
    vertices.push_back(std::move(vx));
    Index ix = static_cast<Index>(vertices.size() - 1);
#ifdef DEBUG
    std::cout << ix << ": {"
      << vertices[ix].x << ", "
      << vertices[ix].y << ", "
      << vertices[ix].z << "}\n";
#endif
    return {ix, true};
  }

  Face sort_ccw(const Face& in) {
    Face out{};
    for(auto ix : in.vertices) {
      if(out.vertices.size() == 0) {
        out.vertices.push_back(ix);
        continue;
      } else if(out.vertices.size() == 1) {
        if(distance(vertices[out.vertices[0]], vertices[ix]) > epsilon)
          out.vertices.push_back(ix);
        continue;
      }

      auto vnew = vertices[ix];
      Index last = out.vertices.back();
      auto vlast = vertices[last];
      for(auto cur : out.vertices) {
        auto vcur = vertices[cur];
        if(dot(cross(vcur - vlast, vnew - vlast), in.normal) > 0) {
          out.vertices.insert(std::find(begin(out.vertices), end(out.vertices), cur), ix);
          break;
        }
        last = cur;
        vlast = vcur;
      }
    }
    out.normal = in.normal;
    return out;
  }

  void cleanup() {
    std::vector<bool> used(faces.size(), false);
    for(const auto& f : faces)
      for(auto ix : f.vertices)
        used[ix] = true;
    std::map<Index, Index> map{};
    size_t removed = 0;
    for(size_t i = 0, j = 0; j < vertices.size(); i++, j++) {
      while(!used[j]) {
#ifdef DEBUG
        std::cout << "Freeing vertex " << j << '\n';
#endif
        j++;
        removed++;
      }
      std::swap(vertices[i], vertices[j]);
      map[j] = i;
    }
    if(removed == 0)
      return;
    vertices.resize(vertices.size() - removed);
    for(auto& f : faces)
      for(auto& ix : f.vertices)
        ix = map[ix];
    dump();
  }

  void dump() {
#ifdef DEBUG
    for(Index ix = 0; ix < vertices.size(); ix++) {
      std::cout << ix << ": {"
        << vertices[ix].x << ", "
        << vertices[ix].y << ", "
        << vertices[ix].z << "}\n";
    }
    for(const auto& f : faces) {
      std::cout << "{ ";
      for(auto ix : f.vertices)
        std::cout << ix << ' ';
      std::cout << "} n {"
        << f.normal.x << ", "
        << f.normal.y << ", "
        << f.normal.z << "}\n";
    }
#endif
  }
};
