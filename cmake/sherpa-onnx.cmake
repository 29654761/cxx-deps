include("${CXX_DEPS}/cmake/base.cmake")

# sherpa-onnx 预编译库(C API)。注意:产物为扁平布局 out/${ABI}-${CONFIG},
SET(SHERPA_ONNX_ROOT "${CXX_BUILD}/sherpa-onnx")


target_include_directories(${PROJECT_NAME} PUBLIC "${SHERPA_ONNX_ROOT}/out/${ABI}-${CONFIG}/include")

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(SHERPA_ONNX_LIBS "${SHERPA_ONNX_ROOT}/out/${ABI}-${CONFIG}/lib/sherpa-onnx-c-api.lib")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${SHERPA_ONNX_LIBS})

    # onnxruntime.dll 是 sherpa-onnx-c-api.dll 的运行时依赖,拷贝到目标输出目录
    set(SHERPA_ONNX_DLLS
        "${SHERPA_ONNX_ROOT}/out/${ABI}-${CONFIG}/lib/sherpa-onnx-c-api.dll"
        "${SHERPA_ONNX_ROOT}/out/${ABI}-${CONFIG}/lib/onnxruntime.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHERPA_ONNX_DLLS} $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
    target_link_libraries(${PROJECT_NAME} PUBLIC "${SHERPA_ONNX_ROOT}/out/android-arm64-v8a/libsherpa-onnx-c-api.so")
endif()
