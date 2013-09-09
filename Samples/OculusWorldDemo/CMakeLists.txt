project(SAMPLE_OculusWorldDemo)

set(EXTRA_LIBS 
	OculusVR
	CommonSrc
	TinyXML2
)

set(SOURCE_FILES 
    OculusWorldDemo.cpp
    Player.cpp
)

if (WIN32)
    add_executable(OculusWorldDemo WIN32 ${SOURCE_FILES})
	add_custom_command(TARGET OculusWorldDemo POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets  ${CMAKE_CURRENT_BINARY_DIR}/Assets)
elseif (APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    list(APPEND EXTRA_LIBS ${COCOA_LIBRARY} ${IOKIT_LIBRARY})
    add_executable(OculusWorldDemo MACOSX_BUNDLE ${SOURCE_FILES})
else()
    add_executable(OculusWorldDemo ${SOURCE_FILES})
	add_custom_command(TARGET OculusWorldDemo POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets  ${CMAKE_CURRENT_BINARY_DIR}/Assets)
endif()

target_link_libraries(OculusWorldDemo ${EXTRA_LIBS})
