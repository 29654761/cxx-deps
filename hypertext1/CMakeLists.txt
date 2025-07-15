

project ("hypertext")

option(HYPERTEXT_CURL "Build with curl" OFF)

if(HYPERTEXT_CURL)
    add_definitions(-DNOMINMAX)
endif()


file(GLOB_RECURSE SRCS *.cpp *.c *.cc)

add_library (${PROJECT_NAME} STATIC ${SRCS})

#if(HYPERTEXT_CURL)
    include("${CXX_DEPS}/cmake/curl.cmake")
#endif()
target_link_libraries(${PROJECT_NAME} PRIVATE
    sys2
)


set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)