#include "Shader.h"

Shader::Shader(Entity &owner, const std::string &path) : Component(owner)
{
    std::string vertexPath = path + ".vert";
    std::string fragmentPath = path + ".frag";
    FILE* vs = fopen(vertexPath.c_str() , "rb");
    FILE* fs = fopen(fragmentPath.c_str() , "rb");
    if (!vs || !fs)
    {
        printf("%s \bERROR: Failed to compile shader file. File does not exist.\n", ERROR);
        return;
    }
    fseek(vs, 0, SEEK_END);
    int size = ftell(vs);
    rewind(vs);
    auto src = new GLchar[size-1];
    fread(static_cast<char*>(src), sizeof(char), size, vs);
    static_cast<char&>(src[size]) = '\0';

    const GLint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &src, nullptr);
    glCompileShader(vertexShader);

    fseek(fs, 0, SEEK_END);
    size = ftell(fs);
    rewind(fs);
    src = new GLchar[size-1];
    fread(static_cast<char*>(src), sizeof(char), size, fs);
    static_cast<char&>(src[size]) = '\0';

    const GLint fragmentShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(fragmentShader, 1, &src, nullptr);
    glCompileShader(fragmentShader);

    const GLint program = glCreateProgram();
    if (!program)
    {
        printf("%s \bERROR: Failed to create shader program.\n", ERROR);
    } else {
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint linked;
        glGetShaderiv(program, GL_LINK_STATUS, &linked);

        if (!linked)
        {
            printf("%s \bERROR: Failed to link shader program with shaders.\n", ERROR);
        } else {
            *m_Shader = program;
        }
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Shader::render() const
{
    if (m_Shader)
    {
        glUseProgram(*m_Shader);
        glBegin(GL_QUADS);
        glTranslatef(0.f, 0., 0.f);
        glVertex2f(0.f, 0.5f);
        glVertex2f(0.5f, -0.5f);
        glVertex2f(-0.5f, -0.5f);
        glEnd();
        glFlush();
    }
}