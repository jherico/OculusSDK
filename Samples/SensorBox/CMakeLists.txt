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

    add_executable(SensorBoxTest MACOSX_BUNDLE ${SOURCE_FILES})

else()

    add_executable(SensorBoxTest ${SOURCE_FILES})

endif()

target_link_libraries(SensorBoxTest ${EXTRA_LIBS})
