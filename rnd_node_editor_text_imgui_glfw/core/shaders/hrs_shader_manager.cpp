#include "glad/glad.h"

#include "hrs_shader_manager.h"

#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include <iostream>

static void load_file(std::string const& name, std::vector<char>& contents, bool binary = false)
{
	std::ios_base::openmode open_mode = std::ios::in;
	if (binary) {
		open_mode |= std::ios::binary;
	}

	std::ifstream file(name, open_mode);
	if (!file.is_open()) 
	{

		exit(-1);
	}

	contents.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static GLuint compile_shader(GLenum type, std::vector<GLchar> const& source)
{
	GLuint shader = glCreateShader(type);

	GLint len = static_cast<GLint>(source.size());
	GLchar const* source_array = &source[0];

	glShaderSource(shader, 1, &source_array, &len);
	glCompileShader(shader);

	GLint result = GL_TRUE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

	if (result == GL_FALSE)
	{
		std::string log(len, '\0');
		glGetShaderInfoLog(shader, len, &result, log.data());
		glDeleteShader(shader);
		throw std::runtime_error(log);
	}

	return shader;
}

GLuint ShaderManager::compile_program(std::string const& prog_name)
{
	std::string vsName = prog_name + ".vert";
	std::string fsName = prog_name + ".frag";

	auto load_and_compile_shader = [](const std::string& filename, GLenum shader_type) {
		std::vector<GLchar> source_code;
		load_file(filename, source_code);
		return compile_shader(shader_type, source_code);
		};

	GLuint vertex_shader = load_and_compile_shader(vsName, GL_VERTEX_SHADER);
	GLuint frag_shader = load_and_compile_shader(fsName, GL_FRAGMENT_SHADER);

	GLuint program = glCreateProgram();

	glAttachShader(program, vertex_shader);
	glAttachShader(program, frag_shader);

	glDeleteShader(vertex_shader);
	glDeleteShader(frag_shader);

	glLinkProgram(program);

	GLint result = GL_TRUE;
	glGetProgramiv(program, GL_LINK_STATUS, &result);

	if (result == GL_FALSE)
	{
		GLint length = 0;
		std::vector<char> log;

		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

		log.resize(length);

		glGetProgramInfoLog(program, length, &result, &log[0]);

		glDeleteProgram(program);

		throw std::runtime_error(std::string(log.begin(), log.end()));
	}

	return program;
}

GLuint ShaderManager::get_program(std::string const& prog_name)
{
	auto iter = programs_.find(prog_name);

	if (iter != programs_.end())
	{
		return iter->second;
	}
	else
	{
		GLuint program = compile_program(prog_name);
		programs_[prog_name] = program;
		return program;
	}
}