project(CommonSrc)

file(GLOB SOURCE_FILES 
    Util/*.h Util/*.cpp
    Platform/Platform*
    Platform/Gamepad*
    Render/Render_Device*
    Render/Render_Font*
    Render/Render_Load*
    Render/Render_Xml*
    Render/Render_GL_Device*
)

if(WIN32)
    file(GLOB PLATFORM_SOURCE_FILES 
        Platform/Win32*
        Render/Render_GL_Win32*
        Render/Render_D3D*
    )
elseif(APPLE)
    file(GLOB PLATFORM_SOURCE_FILES 
        Platform/OSX*
    )
else() 
    file(GLOB PLATFORM_SOURCE_FILES 
        Platform/Linux*
    )
endif()


add_library (CommonSrc STATIC ${SOURCE_FILES} ${PLATFORM_SOURCE_FILES})
