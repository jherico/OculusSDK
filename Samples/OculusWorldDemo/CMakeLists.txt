project(OculusWorldDemo)

set(EXTRA_LIBS 
	OculusVR
	CommonSrc
	TinyXML2
)

file(GLOB SOURCE_FILES *.cpp *.h)

file(GLOB_RECURSE ASSET_FILES Assets/Tuscany/*)

if (WIN32)
    find_package(DirectX REQUIRED)
    list(APPEND EXTRA_LIBS 
        ${DirectX_D3D10_LIBRARY} 
        ${DirectX_D3DX10_LIBRARY} 
        ${DirectX_D3D10_1_LIBRARY} 
        ${DirectX_D3DX10_LIBRARY} 
        ${DirectX_D3D11_LIBRARY} 
        ${DirectX_D3DX11_LIBRARY} 
        d3dcompiler
        dxgi
    )
    find_package(OpenGL REQUIRED)
    list(APPEND EXTRA_LIBS ${OPENGL_LIBRARIES})

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

get_cmake_property(_variableNames VARIABLES)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()
#message(FATAL_ERROR ${EXTRA_LIBS})
target_link_libraries(OculusWorldDemo ${EXTRA_LIBS})
