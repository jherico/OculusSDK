project(SAMPLE_CommonSrc)

set(SOURCE_FILES
    Platform/Platform.cpp
    Platform/Platform.cpp
    Render/Render_Device.cpp
    Render/Render_LoadTextureDDS.cpp
    Render/Render_LoadTextureTGA.cpp
    Render/Render_XmlSceneLoader.cpp
)

if(WIN32)
    list(APPEND SOURCE_FILES 
        Platform/Win32_Platform.cpp
        Platform/Win32_Gamepad.cpp
        Render/Render_D3D10_Device.cpp
        Render/Render_D3D11_Device.cpp
    )

	find_package(DirectX REQUIRED)
    list(APPEND CommonSrc_EXTRA_LIBS 
    	${DirectX_D3D10_LIBRARY}
    	${DirectX_D3D11_LIBRARY}
    	${DirectX_LIB_SEARCH_PATH}/d3dcompiler.lib
    	${DirectX_LIB_SEARCH_PATH}/dxgi.lib
	)

elseif(APPLE)
    list(APPEND SOURCE_FILES 
        Platform/OSX_Platform.mm
        Platform/OSX_Gamepad.cpp
        Platform/OSX_WavPlayer.cpp
	    Render/Render_GL_Device.cpp
    )

    # FIXME ??? need to add libraries here
else()
    list(APPEND SOURCE_FILES 
        Platform/Linux_Platform.cpp
        Platform/Linux_Gamepad.cpp
	    Render/Render_GL_Device.cpp
    )

    list(APPEND CommonSrc_EXTRA_LIBS GL GLU glut) 
endif()

add_library (CommonSrc STATIC ${SOURCE_FILES})
target_link_libraries(CommonSrc ${CommonSrc_EXTRA_LIBS})
