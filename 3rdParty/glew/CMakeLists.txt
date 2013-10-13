project (GLEW C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(glew STATIC glew.c include/GL/glew.h)
