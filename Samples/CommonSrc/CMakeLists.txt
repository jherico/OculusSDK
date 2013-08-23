project(CommonSrc_LIB)


set(SOURCE_FILES
    Platform/Platform.cpp
    Platform/Platform.cpp
    Render/Render_GL_Device.cpp
    Render/Render_Device.cpp
    Render/Render_LoadTextureDDS.cpp
    Render/Render_LoadTextureTGA.cpp
    Render/Render_XmlSceneLoader.cpp
)

if(WIN32)
    list(APPEND SOURCE_FILES 
        Platform/Win32_Gamepad.cpp
        Platform/Win32_Platform.cpp
        Render/Render_D3D10_Device.cpp
        Render/Render_D3D11_Device.cpp
        Render/Render_D3D1X_Device.cpp
        Render/Render_GL_Win32_Device.cpp
    )
elseif(APPLE)
    list(APPEND SOURCE_FILES 
        Platform/OSX_Gamepad.cpp
        Platform/OSX_WavPlayer.cpp
    )
else()
    list(APPEND SOURCE_FILES 
        Platform/Linux_Gamepad.cpp
        Platform/Linux_Platform.cpp
    )
endif()

add_library (CommonSrc STATIC ${SOURCE_FILES})
