# Turns a shader file into a C++ header holding its source as a string, so
# games never have to find shader files at run time.
#
#   cmake -DSOURCE=<shader> -DOUTPUT=<header> -DNAME=<identifier> -P EmbedShader.cmake

file(READ "${SOURCE}" CONTENT)
get_filename_component(SOURCE_NAME "${SOURCE}" NAME)

file(WRITE "${OUTPUT}.tmp"
        "// Generated from ${SOURCE_NAME} by cmake/EmbedShader.cmake. Edit the shader, not this.\n"
        "#pragma once\n\n"
        "inline constexpr const char ${NAME}[] = R\"maple_shader(${CONTENT})maple_shader\";\n")
# Only touch the header when the shader changed, so nothing rebuilds needlessly.
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
