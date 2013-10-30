project(TestSensorC C)

set(EXTRA_LIBS 
    OculusVR
)

set(SOURCE_FILES 
    TestSensor.c
)

add_executable(TestSensorC ${SOURCE_FILES})
target_link_libraries(TestSensorC ${EXTRA_LIBS})
