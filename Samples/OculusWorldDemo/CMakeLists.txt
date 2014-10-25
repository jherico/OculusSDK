project(OculusWorldDemo)

file(GLOB SOURCE_FILES *.cpp *.h)
file(GLOB_RECURSE ASSET_FILES Assets/Tuscany/*)

# All the Oculus VR functionality comes from this DLL
set(EXTRA_LIBS OculusVR)
# While the samples use the C API, they also use the 
# core SDK functionality quite a bit
list(APPEND EXTRA_LIBS ${OVR_LIBRARIES})

include_directories(../../3rdParty/glext)

# Supplementary rendering wrapper functionality
list(APPEND EXTRA_LIBS CommonSrc)

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
    # Screen manipulation
    list(APPEND EXTRA_LIBS Xinerama)

    add_executable(OculusWorldDemo ${SOURCE_FILES})
	add_custom_command(TARGET OculusWorldDemo POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets  ${CMAKE_CURRENT_BINARY_DIR}/Assets)
endif()

# Scene loading
list(APPEND EXTRA_LIBS TinyXML2)
find_package(OpenGL REQUIRED)
list(APPEND EXTRA_LIBS ${OPENGL_LIBRARIES})

if (WIN32)
    find_package( DirectX REQUIRED)
    list(APPEND EXTRA_LIBS ${DirectX_D3D10_LIBRARY})
    list(APPEND EXTRA_LIBS ${DirectX_D3DX10_LIBRARY})
    list(APPEND EXTRA_LIBS ${DirectX_D3D10_1_LIBRARY})
    list(APPEND EXTRA_LIBS ${DirectX_D3D11_LIBRARY})
    list(APPEND EXTRA_LIBS ${DirectX_D3DX11_LIBRARY})
    list(APPEND EXTRA_LIBS d3dcompiler DXGI)
endif()

target_link_libraries(OculusWorldDemo ${EXTRA_LIBS})
