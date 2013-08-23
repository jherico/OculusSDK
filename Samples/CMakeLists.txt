include_directories(${OculusVR_LIB_SOURCE_DIR}/Include)
include_directories(${OculusVR_LIB_SOURCE_DIR}/Src)
include_directories(${TinyXML2_LIB_SOURCE_DIR})

add_definitions( -DGL_GLEXT_PROTOTYPES )

if(WIN32)
    # ???
elseif(APPLE)
    # ???
    FIND_LIBRARY(COCOA_LIBRARY Cocoa)
    FIND_LIBRARY(GLUT_LIBRARY GLUT )
    FIND_LIBRARY(OpenGL_LIBRARY OpenGL )
else()
    SET(OculusVR_EXTRA_LIBS pthread udev GL GLU glut X11 Xinerama)
endif()

add_subdirectory (CommonSrc)
add_subdirectory (OculusWorldDemo)
#add_subdirectory (OculusRoomTiny)
