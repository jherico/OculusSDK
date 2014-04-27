project(CommonSrc)

file(GLOB_RECURSE SOURCE_FILES *.h *.cpp)

add_library (CommonSrc STATIC ${SOURCE_FILES})
