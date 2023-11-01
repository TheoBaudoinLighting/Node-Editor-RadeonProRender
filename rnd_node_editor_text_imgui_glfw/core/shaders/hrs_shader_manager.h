#pragma once

#define NOMINMAX
#include <glad/glad.h>

#include <string>
#include <map>

class ShaderManager
{
public:

	ShaderManager() {}

	~ShaderManager()
	{
		for (auto shader = programs_.cbegin(); shader != programs_.cend(); ++shader)
		{
			glDeleteProgram(shader->second);
		}
	}

	GLuint get_program(std::string const& prog_name);

private:

	GLuint compile_program(std::string const& prog_name);

	ShaderManager(ShaderManager const&);
	ShaderManager& operator=(ShaderManager const&);

	std::map<std::string, GLuint> programs_;
};

