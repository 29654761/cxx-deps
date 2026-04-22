

include("${CXX_DEPS}/cmake/base.cmake")


SET(ROOT "${CXX_BUILD}/GPUPixel")

message("ROOT: ${ROOT}")
message("ABI: ${ABI}")
message("CONFIG: ${CONFIG}")


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(LIBS
        "${ROOT}/out/${ABI}-${CONFIG}/lib/gpupixel.lib"
    )
else()
    set(LIBS
        "${ROOT}/out/${ABI}-${CONFIG}/lib/gpupixel.so"
    )
endif()


target_include_directories(${PROJECT_NAME} PUBLIC
    ${ROOT}/out/${ABI}-${CONFIG}/include
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    ${LIBS}
)


