project(SensorBoxTest)

set(EXTRA_LIBS 
	OculusVR
	CommonSrc
)

set(SOURCE_FILES 
    SensorBoxTest.cpp
)

if (WIN32)

    add_executable(SensorBoxTest WIN32 ${SOURCE_FILES})

elseif (APPLE)

    find_library(OpenGL_LIBRARY OpenGL)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    list(APPEND EXTRA_LIBS ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${OpenGL_LIBRARY})
    add_executable(SensorBoxTest MACOSX_BUNDLE ${SOURCE_FILES})

else()

    add_executable(SensorBoxTest ${SOURCE_FILES})

endif()

target_link_libraries(SensorBoxTest ${EXTRA_LIBS})
