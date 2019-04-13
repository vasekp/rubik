#ifndef GLUTIL_HPP
#define GLUTIL_HPP

#include <GL/glew.h>
#include <fstream>
#include <sstream>
#include <initializer_list>
#include <array>

namespace GLutil {

  using namespace std::string_literals;

  constexpr std::size_t max_error_length = 1000;


  inline void initGLEW() {
    if(int err = glewInit(); err != GLEW_OK) {
      std::ostringstream oss{};
      oss << "glewInit: " << glewGetErrorString(err);
      throw std::runtime_error(oss.str());
    }
  }


  inline std::string readfile(std::string filename) {
    std::ifstream file{filename};
    std::ostringstream buffer{};
    buffer << file.rdbuf();
    return buffer.str();
  }


  class shader {
    private:
    GLenum type;
    GLuint shaderObject;

    public:
    enum source_e {
      from_source,
      from_file
    };

    shader() : shaderObject(0) { }

    shader(const char* source, GLenum type_, source_e src = from_source)
      : type(type_) 
    {
      shaderObject = glCreateShader(type);
      if(src == from_source)
        glShaderSource(shaderObject, 1, &source, nullptr);
      else {
        std::string contents = readfile(source);
        const char *text = contents.data();
        glShaderSource(shaderObject, 1, &text, nullptr);
      }
      glCompileShader(shaderObject);

      GLint ret;
      glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &ret);
      if(ret == GL_FALSE) {
        std::array<char, max_error_length> log;
        GLsizei len;
        glGetShaderInfoLog(shaderObject, max_error_length, &len, log.data());
        log[len] = '\0';
        throw std::runtime_error("Error: "s
            + describeType()
            + " shader did not compile:\n"s
            + std::string(log.data()));
      }
    }

    shader(const shader&) = delete;

    shader(shader&& other) {
      swap(*this, other);
    }

    const shader& operator=(const shader&) = delete;

    const shader& operator=(shader&& other) {
      swap(*this, other);
      return *this;
    }

    friend void swap(shader& a, shader& b) {
      std::swap(a.shaderObject, b.shaderObject);
      std::swap(a.type, b.type);
    }

    ~shader() {
      if(shaderObject != 0)
        glDeleteShader(shaderObject);
    }

    operator GLuint() const {
      return shaderObject;
    }

    private:
    std::string describeType() const {
      switch(type) {
        case GL_VERTEX_SHADER:
          return "vertex";
        case GL_TESS_CONTROL_SHADER:
          return "tesselation control";
        case GL_TESS_EVALUATION_SHADER:
          return "tesselation evaluation";
        case GL_GEOMETRY_SHADER:
          return "geometry";
        case GL_FRAGMENT_SHADER:
          return "fragment";
        case GL_COMPUTE_SHADER:
          return "compute";
        default:
          return "unknown";
      }
    }

    GLbitfield getProgramBit() const {
      switch(type) {
        case GL_VERTEX_SHADER:
          return GL_VERTEX_SHADER_BIT;
        case GL_TESS_CONTROL_SHADER:
          return GL_TESS_CONTROL_SHADER_BIT;
        case GL_TESS_EVALUATION_SHADER:
          return GL_TESS_EVALUATION_SHADER_BIT;
        case GL_GEOMETRY_SHADER:
          return GL_GEOMETRY_SHADER_BIT;
        case GL_FRAGMENT_SHADER:
          return GL_FRAGMENT_SHADER_BIT;
        case GL_COMPUTE_SHADER:
          return GL_COMPUTE_SHADER_BIT;
        default:
          return 0;
      }
    }

    friend class program;
  };


  class program {
    private:
    GLuint programObject;
    GLbitfield stages;

    public:
    program() : programObject(0) { }

    program(std::initializer_list<shader> shaders) :
      program(false, shaders)
    { }

    program(bool separate, std::initializer_list<shader> shaders) {
      stages = 0;
      programObject = glCreateProgram();
      for(const auto& shader : shaders) {
        glAttachShader(programObject, shader);
        stages |= shader.getProgramBit();
      }
      if(separate)
        glProgramParameteri(programObject, GL_PROGRAM_SEPARABLE, GL_TRUE);
      glLinkProgram(programObject);

      GLint ret;
      glGetProgramiv(programObject, GL_LINK_STATUS, &ret);
      if(ret == GL_FALSE) {
        std::array<char, max_error_length> log;
        GLsizei len;
        glGetProgramInfoLog(programObject, max_error_length, &len, log.data());
        log[len] = '\0';
        throw std::runtime_error("Error: "s
            + "program did not link:\n"s
            + std::string(log.data()));
      }
    }

    program(const program&) = delete;

    program(program&& other) {
      swap(*this, other);
    }

    const program& operator=(const program&) = delete;

    const program& operator=(program&& other) {
      swap(*this, other);
      return *this;
    }

    friend void swap(program& a, program& b) {
      std::swap(a.programObject, b.programObject);
      std::swap(a.stages, b.stages);
    }

    ~program() {
      if(programObject != 0)
        glDeleteProgram(programObject);
    }

    operator GLuint() const {
      return programObject;
    }

    GLbitfield getStages() const {
      return stages;
    }
  };


  class program_separate : public program {
    using program::program;

    public:
    program_separate(std::initializer_list<shader> shaders)
      : program(true, shaders)
    { }
  };

}

#endif
