

include("${CXX_DEPS}/cmake/base.cmake")

SET(FREETYPE_ROOT "${CXX_BUILD}/freetype")
SET(HARFBUZZ_ROOT "${CXX_BUILD}/harfbuzz")
SET(OPENCV_ROOT "${CXX_BUILD}/opencv")

message("OPENCV_ROOT: ${OPENCV_ROOT}")
message("ABI: ${ABI}")
message("CONFIG: ${CONFIG}")
message("BUILD_WITH_CUDA":${BUILD_WITH_CUDA})




if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${OPENCV_ROOT}/out/${ABI}-${CONFIG}/include"
    )

    file(GLOB_RECURSE LIBS "${OPENCV_ROOT}/out/${ABI}-${CONFIG}/${ABI}/vc17/staticlib/*.lib")

    if(${CONFIG} STREQUAL "debug")
        set(FREETYPE_LIB "${FREETYPE_ROOT}/out/${ABI}-${CONFIG}/lib/freetyped.lib")
    else()
        set(FREETYPE_LIB "${FREETYPE_ROOT}/out/${ABI}-${CONFIG}/lib/freetype.lib")
    endif()
    set(HARFBUZZ_LIB "${HARFBUZZ_ROOT}/out/${ABI}-${CONFIG}/lib/harfbuzz.lib")
else()
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${OPENCV_ROOT}/out/${ABI}-${CONFIG}/include/opencv4"
    )
    file(GLOB_RECURSE LIBS "${OPENCV_ROOT}/out/${ABI}-${CONFIG}/lib/*.a")
    if(${CONFIG} STREQUAL "debug")
        set(FREETYPE_LIB "${FREETYPE_ROOT}/out/${ABI}-${CONFIG}/lib/libfreetyped.a")
    else()
        set(FREETYPE_LIB "${FREETYPE_ROOT}/out/${ABI}-${CONFIG}/lib/libfreetype.a")
    endif()
    set(HARFBUZZ_LIB "${HARFBUZZ_ROOT}/out/${ABI}-${CONFIG}/lib/libharfbuzz.a")
    if(BUILD_WITH_CUDA)
        set(CUDA_LIBs 
            /usr/local/cuda/lib64/libcudart_static.a
            /usr/local/cuda/lib64/libcudadevrt.a
            /usr/local/cuda/lib64/libcublas_static.a
            /usr/local/cuda/lib64/libcusolver_static.a)
    endif()
endif()

message("============>OpenCVLibs:${LIBS}")

target_link_libraries(${PROJECT_NAME} PUBLIC
    -Wl,--start-group ${LIBS} -Wl,--end-group
    ${FREETYPE_LIB}
    ${HARFBUZZ_LIB}
)

if(BUILD_WITH_CUDA)
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${CUDA_LIBs}
)
endif()
