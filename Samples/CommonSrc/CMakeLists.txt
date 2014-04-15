project(CommonSrc)

file(GLOB_RECURSE SOURCE_FILES *.h *.cpp)

add_library (CommonSrc STATIC ${SOURCE_FILES})
target_link_libraries(CommonSrc ${CommonSrc_EXTRA_LIBS})
