add_executable(OculusWorldDemo 
    OculusWorldDemo.cpp
    Player.cpp
)

FILE(GLOB RES_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Tuscany/*")
INSTALL(FILES ${RES_FILES} 
    DESTINATION share/oculus_vr/OculusWorldDemo/Assets/Tuscany
)

INSTALL(TARGETS OculusWorldDemo 
  RUNTIME DESTINATION share/oculus_vr/OculusWorldDemo
)

target_link_libraries(OculusWorldDemo OculusVR_static CommonSrc TinyXML2_embedded ${OculusVR_EXTRA_LIBS})
