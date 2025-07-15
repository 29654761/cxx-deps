

project ("litertp")
set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRCS *.cpp *.c *.cc)

option(LITERTP_SSL "Enable SSL support" OFF)
option(LITERTP_SCTP "Enable SCTP support" OFF)



add_library(${PROJECT_NAME} STATIC ${SRCS})


target_compile_definitions(${PROJECT_NAME} PUBLIC LITERTP_STATIC NOMINMAX)

message("LITERTP_SSL=${LITERTP_SSL}")
if(LITERTP_SSL)
    target_compile_definitions(${PROJECT_NAME} PUBLIC LITERTP_SSL)
    include("${CXX_DEPS}/cmake/openssl.cmake")
    include("${CXX_DEPS}/cmake/srtp.cmake")
endif()



if(LITERTP_SCTP)
    target_compile_definitions(${PROJECT_NAME} PUBLIC LITERTP_SCTP)
    include("${CXX_DEPS}/cmake/sctp.cmake")
endif()


target_link_libraries(${PROJECT_NAME} PUBLIC
    sys2
)



