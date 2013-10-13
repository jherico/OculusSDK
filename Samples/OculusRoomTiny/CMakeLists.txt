set(EXE_NAME OculusRoomTiny)

project(SAMPLE_${EXE_NAME})

set(SOURCE_FILES
    OculusRoomTiny.cpp
    OculusRoomTiny.h
    OculusRoomModel.cpp
    RenderTiny_Device.cpp
    RenderTiny_Device.h
    RenderTiny_GL_Device.cpp
    RenderTiny_GL_Device.h
)

set(EXTRA_LIBS 
	OculusVR
	glfw
	glew 
    ${GLFW_LIBRARIES}
)


if(WIN32)

    add_executable(${EXE_NAME} WIN32 ${SOURCE_FILES} )

elseif(APPLE)

    add_executable(${EXE_NAME} MACOSX_BUNDLE ${SOURCE_FILES} )

else()

    add_executable(${EXE_NAME} ${SOURCE_FILES} )

endif()

target_link_libraries(${EXE_NAME} ${EXTRA_LIBS})



