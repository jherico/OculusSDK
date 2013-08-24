project(SAMPLE_OculusWorldDemo)

set(EXTRA_LIBS 
	OculusVR
	CommonSrc
	TinyXML2
)

if (WIN32)
add_executable(OculusWorldDemo WIN32
    OculusWorldDemo.cpp
    Player.cpp
)
elseif (APPLE) 
       FIND_LIBRARY(COCOA_LIBRARY Cocoa)
       FIND_LIBRARY(GLUT_LIBRARY GLUT)
       FIND_LIBRARY(OpenGL_LIBRARY OpenGL)
       find_library(IOKIT_LIBRARY IOKit)
       list(APPEND EXTRA_LIBS
       		   ${COCOA_LIBRARY}
       		   ${IOKIT_LIBRARY}
		   ${GLUT_LIBRARY}
		   ${OpenGL_LIBRARY}
       )
       add_executable(OculusWorldDemo MACOSX_BUNDLE
           OculusWorldDemo.cpp
	   Player.cpp
       )
else()
    add_executable(OculusWorldDemo
         OculusWorldDemo.cpp
         Player.cpp
    )
endif()


target_link_libraries(OculusWorldDemo ${EXTRA_LIBS})

