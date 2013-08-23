set(SOURCE_FILES
    OculusRoomModel.cpp
    RenderTiny_Device.cpp
)

if(WIN32)
    list(APPEND SOURCE_FILES 
        RenderTiny_D3D1X_Device.cpp
        Win32_OculusRoomTiny.cpp
    )
elseif(APPLE)
    list(APPEND SOURCE_FILES 
        RenderTiny_GL_Device.cpp
        OSX_OculusRoomTiny.mm
    )
else()
    list(APPEND SOURCE_FILES 
        RenderTiny_GL_Device.cpp
    )
endif()

add_executable (OculusRoomTiny ${SOURCE_FILES})

target_link_libraries(OculusRoomTiny OculusVR_static ${OculusVR_EXTRA_LIBS})

