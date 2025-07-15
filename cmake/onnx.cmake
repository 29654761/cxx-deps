

include("${CXX_DEPS}/cmake/base.cmake")


SET(ONNX_ROOT "${CXX_BUILD}/onnx-runtime")

message("ONNX_ROOT: ${ONNX_ROOT}")
message("ABI: ${ABI}")
message("CONFIG: ${CONFIG}")





if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    file(GLOB_RECURSE LIBS "${ONNX_ROOT}/out/${ABI}-${CONFIG}/lib/*.lib")
else()
    file(GLOB_RECURSE LIBS "${ONNX_ROOT}/out/${ABI}-${CONFIG}/lib/*.a")
endif()

message("============>ONNXLibs:${LIBS}")

target_include_directories(${PROJECT_NAME} PUBLIC
    ${ONNX_ROOT}/out/${ABI}-${CONFIG}/include/onnxruntime
    ${ONNX_ROOT}/out/${ABI}-${CONFIG}/include/onnxruntime/core/session
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    -Wl,--start-group ${LIBS} -Wl,--end-group
)

