project (OVR_C)

include_directories(Include Src)

# Make sure to set up the OVR include paths
include_directories(../../3rdParty/glew/include)
include_directories(${OculusVR_SOURCE_DIR}/Include)
include_directories(${OculusVR_SOURCE_DIR}/Src)
include_directories(${OculusVR_SOURCE_DIR}/Src/Kernel)
include_directories(${OculusVR_SOURCE_DIR}/Src/Util)

file(GLOB SOURCE_FILES Src/*.cpp Src/*.h Src/GL/*.cpp Src/GL/*.h Include/*.h)
if (WIN32)
file(GLOB WIN32_SOURCE_FILES Src/D3D1X/*.cpp Src/D3D1X/*.h)
list(APPEND SOURCE_FILES ${WIN32_SOURCE_FILES})
endif()
add_library(OVR_C SHARED ${SOURCE_FILES})

set(EXTRA_LIBS OculusVR)
list(APPEND EXTRA_LIBS ${OVR_LIBRARIES})

# All platforms require GLEW and OpenGL
list(APPEND EXTRA_LIBS glew)
find_package(OpenGL REQUIRED)
list(APPEND EXTRA_LIBS ${OPENGL_LIBRARIES})

target_link_libraries(OVR_C ${EXTRA_LIBS})
