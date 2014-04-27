project(OculusWorldDemo)

file(GLOB SOURCE_FILES *.cpp *.h)
file(GLOB_RECURSE ASSET_FILES Assets/Tuscany/*)

if (WIN32)
    add_executable(OculusWorldDemo WIN32 ${SOURCE_FILES})
	add_custom_command(TARGET OculusWorldDemo POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets  ${CMAKE_CURRENT_BINARY_DIR}/Assets)
elseif (APPLE)
    list(APPEND SOURCE_FILES ${ASSET_FILES})
    set_source_files_properties(${ASSET_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/Assets/Tuscany)
    add_executable(OculusWorldDemo MACOSX_BUNDLE ${SOURCE_FILES})
else()
    add_executable(OculusWorldDemo ${SOURCE_FILES})
	add_custom_command(TARGET OculusWorldDemo POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets  ${CMAKE_CURRENT_BINARY_DIR}/Assets)
endif()

# All the Oculus VR functionality comes from this DLL
set(EXTRA_LIBS OVR_C)

# While the samples use the C API, they also use the 
# core SDK functionality quite a bit
list(APPEND EXTRA_LIBS OVR)
list(APPEND EXTRA_LIBS ${OVR_LIBRARIES})

# Supplementary rendering wrapper functionality
list(APPEND EXTRA_LIBS CommonSrc)

# Scene loading
list(APPEND EXTRA_LIBS TinyXML2)

# OpenGL, extension loader + core libraries 
list(APPEND EXTRA_LIBS glew)
find_package(OpenGL REQUIRED)
list(APPEND EXTRA_LIBS ${OPENGL_LIBRARIES})

target_link_libraries(OculusWorldDemo ${EXTRA_LIBS})
