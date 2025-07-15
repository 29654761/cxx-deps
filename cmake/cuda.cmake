

include("${CXX_DEPS}/cmake/base.cmake")

SET(ROOT "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6")



target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/include"
)



if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    file(GLOB_RECURSE LIBS "${ROOT}/lib/${ABI}/*.lib")
else()
    file(GLOB_RECURSE LIBS "${ROOT}/lib/${ABI}/*.a")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${LIBS}
)
