project(TestSensor)

set(EXTRA_LIBS 
    OculusVR
)

set(SOURCE_FILES 
    TestSensor.cpp
)

add_executable(TestSensor ${SOURCE_FILES})
target_link_libraries(TestSensor ${EXTRA_LIBS})
