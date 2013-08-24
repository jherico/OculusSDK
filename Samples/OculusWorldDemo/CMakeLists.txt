project(SAMPLE_OculusWorldDemo)

add_executable(OculusWorldDemo 
    OculusWorldDemo.cpp
    Player.cpp
)

set(EXTRA_LIBS 
	OculusVR
	CommonSrc
	TinyXML2
)



FILE(GLOB RES_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Tuscany/*")
INSTALL(FILES ${RES_FILES} 
    DESTINATION share/oculus_vr/OculusWorldDemo/Assets/Tuscany
)

INSTALL(TARGETS OculusWorldDemo 
  RUNTIME DESTINATION share/oculus_vr/OculusWorldDemo
)



target_link_libraries(OculusWorldDemo ${EXTRA_LIBS})

