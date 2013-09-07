project(SAMPLE_OculusTinyRoom)

set(EXTRA_LIBS 
	OculusVR
)

set(SOURCE_FILES
    OculusRoomTiny.cpp
    OculusRoomModel.cpp
    RenderTiny_Device.cpp
)

if (WIN32)
	find_package(DirectX REQUIRED)
    list(APPEND EXTRA_LIBS 
    	${DirectX_D3D10_LIBRARY}
    	${DirectX_D3D11_LIBRARY}
    	${DirectX_LIB_SEARCH_PATH}/d3dcompiler.lib
    	${DirectX_LIB_SEARCH_PATH}/dxgi.lib
	)

    list(APPEND SOURCE_FILES 
        OculusRoomTiny_Win32.cpp
        RenderTiny_D3D1X_Device.cpp
    )

    add_executable(OculusTinyRoom WIN32 ${SOURCE_FILES})
elseif (APPLE) 

else()
    list(APPEND EXTRA_LIBS 
    	EGL GL X11
    )

    list(APPEND SOURCE_FILES 
        OculusRoomTiny_Linux.cpp
        RenderTiny_GL_Device.cpp
        RenderTiny_GLX_Device.cpp
    )
    add_executable(OculusTinyRoom ${SOURCE_FILES})
endif()

target_link_libraries(OculusTinyRoom ${EXTRA_LIBS})



