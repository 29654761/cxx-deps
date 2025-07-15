

project ("hypertext")



file(GLOB_RECURSE SRCS *.cpp *.c *.cc)

add_library (${PROJECT_NAME} STATIC ${SRCS})


target_link_libraries(${PROJECT_NAME} PRIVATE
    sys2
)

