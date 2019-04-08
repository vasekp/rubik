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
  std::vector<Index> indices;
  glm::vec3 normal;
};

struct Volume {
  std::vector<Face> faces;
};

struct ExtVolume {
  std::vector<Vertex> vertex_coords;
  std::vector<Face> ext_faces;
  std::vector<Face> ext_edges;
  std::vector<Face> ext_vertices;
};

class Mould {
  std::vector<Vertex> vertices;
  std::vector<Volume> volumes;

public:

  Mould(float size = 2) {
    for(float x = -1; x <= 1; x += 2)
    for(float y = -1; y <= 1; y += 2)
    for(float z = -1; z <= 1; z += 2)
      vertices.push_back({size*x, size*y, size*z});
    std::vector<Face> faces{};
    faces.push_back({{0, 2, 3, 1}, {-1, 0, 0}});
    faces.push_back({{4, 5, 7, 6}, {1, 0, 0}});
    faces.push_back({{0, 1, 5, 4}, {0, -1, 0}});
    faces.push_back({{2, 6, 7, 3}, {0, 1, 0}});
    faces.push_back({{0, 4, 6, 2}, {0, 0, -1}});
    faces.push_back({{1, 3, 7, 5}, {0, 0, 1}});
    volumes.push_back({std::move(faces)});
  }

  void cut(Plane p, bool discard = false) {
    std::vector<Volume> nvolumes{};
    for(auto& volume : volumes) {
      Volume volIn{}, volOut{};
      std::vector<Face> facesIn{}, facesOut{};
      Face nIn{}, nOut{};
      nIn.normal = p.normal;
      nOut.normal = -p.normal;
      for(auto& f : volume.faces) {
        unsigned cIn = 0, cOut = 0;
        for(auto ix : f.indices)
          if(Vertex vx = vertices[ix]; vx * p < 0)
            cIn++;
          else if(vx * p > 0)
            cOut++;

        if(cOut == 0)
          volIn.faces.push_back(std::move(f));
        if(cIn == 0)
          volOut.faces.push_back(std::move(f));
        if(cIn == 0 || cOut == 0)
          continue;

        Face fIn{}, fOut{};
        fIn.normal = f.normal;
        fOut.normal = f.normal;
        const Vertex *last = &vertices[f.indices.back()];
        float ldot = *last * p;
        for(auto ix : f.indices) {
          const Vertex *cur = &vertices[ix];
          float dot = *cur * p;
          if(ldot * dot < 0) {
            auto intersect = (dot * *last - ldot * *cur)/(dot - ldot);
            auto [newIx, added] = find_or_append(std::move(intersect));
            fIn.indices.push_back(newIx);
            nIn.indices.push_back(newIx);
            fOut.indices.push_back(newIx);
            nOut.indices.push_back(newIx);
          }
          else if(dot == 0) {
            fIn.indices.push_back(ix);
            nIn.indices.push_back(ix);
            fOut.indices.push_back(ix);
            nOut.indices.push_back(ix);
          }
          if(dot < 0)
            fIn.indices.push_back(ix);
          else if(dot > 0)
            fOut.indices.push_back(ix);
          last = cur;
          ldot = dot;
        }
        volIn.faces.push_back(std::move(fIn));
        volOut.faces.push_back(std::move(fOut));
      }
      volIn.faces.push_back(sort_ccw(nIn));
      volOut.faces.push_back(sort_ccw(nOut));
      nvolumes.push_back(std::move(volIn));
      if(!discard)
        nvolumes.push_back(std::move(volOut));
    }
    swap(volumes, nvolumes);
    cleanup();
    dump();
  }

  std::vector<ExtVolume> get_volumes_clipped(float dist = 0.1) {
    std::vector<ExtVolume> rvolumes{};
    for(const auto& volume : volumes) {
      const auto& faces = volume.faces;
      std::vector<Vertex> nvertices{};
      std::vector<Face> ext_faces{};
      std::vector<Face> ext_edges{};
      std::map<Index, Face> ext_vertices{};

      size_t nvcount = 0;
      for(const auto& f : faces)
        nvcount += f.indices.size();
      nvertices.reserve(nvcount);
      ext_faces.reserve(faces.size());
      ext_edges.reserve(nvcount);

      // copy faces, blow up vertices
      // each face gets a unique set of new vertices
      // new vertex indices predictable
      for(const auto& f : faces) {
        size_t base = nvertices.size();
        size_t newIx = base;
        std::vector<Index> newIxs{};
        for(auto ix : f.indices) {
          nvertices.push_back(vertices[ix]); // new element @ newIx
          ext_vertices[ix].indices.push_back(newIx);
          ext_vertices[ix].normal += f.normal;
          newIxs.push_back(newIx++);
        }
        ext_faces.push_back({newIxs, f.normal});
      }

      // find coincident edges, make edge faces
      for(size_t fc = faces.size(), fi1 = 0; fi1 < fc; fi1++) {
        const auto& f1 = faces[fi1];
        size_t vc1 = f1.indices.size();
        for(size_t vi1 = 0; vi1 < vc1; vi1++) {
          size_t vj1 = (vi1 + 1) % vc1;
          Index vix1 = f1.indices[vi1],
                vjx1 = f1.indices[vj1];
          bool found = false;
          // [find vi1->vj1 edge in another face]
          for(size_t fi2 = fi1 + 1; !found && fi2 < fc; fi2++) {
            const auto& f2 = faces[fi2];
            size_t vc2 = f2.indices.size();
            for(size_t vi2 = 0; vi2 < vc2; vi2++) {
              size_t vj2 = (vi2 + 1) % vc2;
              Index vix2 = f2.indices[vi2],
                    vjx2 = f2.indices[vj2];
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
                  ext_faces[fi1].indices[vj1],
                  ext_faces[fi1].indices[vi1],
                  ext_faces[fi2].indices[vj2],
                  ext_faces[fi2].indices[vi2]
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
        size_t size = f.indices.size();
        auto* vprev = &nvertices[f.indices.back()];
        auto* vthis = &nvertices[f.indices.front()];
        std::vector<Vertex> shrunk(size);
        for(size_t i = 0; i < size; i++) {
          auto* vnext = &nvertices[f.indices[(i + 1) % size]];
          auto dprev = normalize(*vthis - *vprev);
          auto dnext = normalize(*vnext - *vthis);
          auto cross = glm::cross(dprev, normal);
          auto triple = dot(dnext, cross);
#ifdef DEBUG
          std::cout << "Vertex " << f.indices[i] << ": "
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
          std::cout << "Vertex " << f.indices[i] << " shrunk by "
            << distance(nvertices[f.indices[i]], shrunk[i]) << '\n';
#endif
          nvertices[f.indices[i]] = shrunk[i];
        }
      }

      // fix all new faces
      swap(vertices, nvertices); // TODO remove
      for(auto& f : ext_edges)
        f.normal = normalize(f.normal);
        // ext edge faces already CCW by construction
      for(auto& [ix, f] : ext_vertices) {
        f.normal = normalize(f.normal);
        f = sort_ccw(f);
      }

      std::vector<Face> ext_vertices_v{};
      ext_vertices_v.reserve(ext_vertices.size());
      for(auto& [ix, f] : ext_vertices)
        ext_vertices_v.push_back(std::move(f));

      rvolumes.push_back({std::move(vertices), std::move(ext_faces), std::move(ext_edges), std::move(ext_vertices_v)});
      swap(vertices, nvertices); // TODO remove
    }
    return rvolumes;
  }

private:
  constexpr static float epsilon = 0.01;

  std::pair<Index, bool> find_or_append(Vertex&& vx) {
    for(const auto& vy : vertices)
      if(distance(vx, vy) < epsilon)
        return {&vy - &vertices[0], false};
    // else
    vertices.push_back(std::move(vx));
    Index ix = static_cast<Index>(vertices.size() - 1);
#ifdef DEBUG
    std::cout << "Adding " << ix << ": {"
      << vertices[ix].x << ", "
      << vertices[ix].y << ", "
      << vertices[ix].z << "}\n";
#endif
    return {ix, true};
  }

  Face sort_ccw(const Face& in) {
    Face out{};
    for(auto ix : in.indices) {
      if(out.indices.size() == 0) {
        out.indices.push_back(ix);
        continue;
      } else if(out.indices.size() == 1) {
        if(distance(vertices[out.indices[0]], vertices[ix]) > epsilon)
          out.indices.push_back(ix);
        continue;
      }

      auto vnew = vertices[ix];
      Index last = out.indices.back();
      auto vlast = vertices[last];
      for(auto cur : out.indices) {
        auto vcur = vertices[cur];
        if(dot(cross(vcur - vlast, vnew - vlast), in.normal) > 0) {
          out.indices.insert(std::find(begin(out.indices), end(out.indices), cur), ix);
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
    for(auto& v : volumes)
      v.faces.erase(std::remove_if(begin(v.faces), end(v.faces),
          [](const Face& f) -> bool { return f.indices.empty(); }),
        v.faces.end());

    volumes.erase(std::remove_if(begin(volumes), end(volumes),
        [](const Volume& v) -> bool { return v.faces.empty(); }),
      volumes.end());

    std::vector<bool> used(vertices.size(), false);
    for(const auto& v : volumes)
      for(const auto& f : v.faces)
        for(auto ix : f.indices)
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
    for(auto& v : volumes)
      for(auto& f : v.faces)
        for(auto& ix : f.indices)
          ix = map[ix];
  }

  void dump() {
#ifdef DEBUG
    std::cout << " === \n";
    for(Index ix = 0; ix < vertices.size(); ix++) {
      std::cout << ix << ": {"
        << vertices[ix].x << ", "
        << vertices[ix].y << ", "
        << vertices[ix].z << "}\n";
    }
    for(const auto& v : volumes) {
      std::cout << "Volume:\n";
      for(const auto& f : v.faces) {
        std::cout << "{ ";
        for(auto ix : f.indices)
          std::cout << ix << ' ';
        std::cout << "} n {"
          << f.normal.x << ", "
          << f.normal.y << ", "
          << f.normal.z << "}\n";
      }
    }
#endif
  }
};
