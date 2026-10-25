# Turns any file into a C++ header holding its bytes, so games never have to
# find the file at run time. Like EmbedShader.cmake, for files that are not
# text, such as images.
#
#   cmake -DSOURCE=<file> -DOUTPUT=<header> -DNAME=<identifier> -P EmbedBinary.cmake

file(READ "${SOURCE}" CONTENT HEX)
get_filename_component(SOURCE_NAME "${SOURCE}" NAME)
string(LENGTH "${CONTENT}" HEX_LENGTH)
math(EXPR SIZE "${HEX_LENGTH} / 2")

# Every two hex digits become one byte, sixteen bytes a line.
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BYTES "${CONTENT}")
string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){16})" "\\1\n    " BYTES "${BYTES}")

file(WRITE "${OUTPUT}.tmp"
        "// Generated from ${SOURCE_NAME} by cmake/EmbedBinary.cmake. Edit the file, not this.\n"
        "#pragma once\n\n"
        "#include <cstddef>\n\n"
        "inline constexpr unsigned char ${NAME}[] = {\n    ${BYTES}\n};\n"
        "inline constexpr std::size_t ${NAME}_SIZE = ${SIZE};\n")
# Only touch the header when the file changed, so nothing rebuilds needlessly.
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
