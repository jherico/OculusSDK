project(TestSensor)

set(EXTRA_LIBS 
    OculusVR
)

set(SOURCE_FILES 
    TestSensor.cpp
)

if (APPLE)

    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    list(APPEND EXTRA_LIBS ${COCOA_LIBRARY} ${IOKIT_LIBRARY} )

endif()

add_executable(TestSensor ${SOURCE_FILES})
target_link_libraries(TestSensor ${EXTRA_LIBS})
