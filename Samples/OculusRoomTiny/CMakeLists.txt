project(OculusRoomTiny)

set(SOURCE_FILES
#    MessageBox.cpp
#    MessageBox.h
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
	glew 
	glfw
    ${GLFW_LIBRARIES}
)

if(WIN32)

    add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES} )

elseif(APPLE)

    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCE_FILES} )

else()

    add_executable(${PROJECT_NAME} ${SOURCE_FILES} )

endif()

target_link_libraries(${PROJECT_NAME} ${EXTRA_LIBS})



