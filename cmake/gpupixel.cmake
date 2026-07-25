

include("${CXX_DEPS}/cmake/base.cmake")


SET(ROOT "${CXX_BUILD}/GPUPixel")

message("ROOT: ${ROOT}")
message("ABI: ${ABI}")
message("CONFIG: ${CONFIG}")


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    file(GLOB_RECURSE LIBS "${ROOT}/out/${ABI}-${CONFIG}/lib/*.lib")
else()
    file(GLOB_RECURSE LIBS "${ROOT}/out/${ABI}-${CONFIG}/lib/*.a")
endif()



target_include_directories(${PROJECT_NAME} PUBLIC
    ${ROOT}/out/${ABI}-${CONFIG}/include
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    ${LIBS}
)


