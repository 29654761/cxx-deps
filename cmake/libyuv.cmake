

include("${CXX_DEPS}/cmake/base.cmake")


SET(ROOT "${CXX_BUILD}/libyuv/out/${ABI}-${CONFIG}")
message("LIBYUV Path=${ROOT} For ${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/include"
)



if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    file(GLOB_RECURSE LIBS "${ROOT}/lib/*.lib")
else()
    file(GLOB_RECURSE LIBS "${ROOT}/lib/*.a")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${LIBS}
)
