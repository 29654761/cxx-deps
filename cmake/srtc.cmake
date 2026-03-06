

include("${CXX_DEPS}/cmake/base.cmake")





if (CMAKE_SYSTEM_NAME MATCHES "Windows")

    target_include_directories(${PROJECT_NAME} PUBLIC
        "${CXX_BUILD}/srtc/windows-amd64"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${CXX_BUILD}/srtc/windows-amd64/librtc.lib"
    )

elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")

    # 输出架构方便调试
    message(STATUS "Linux Processor: ${CMAKE_SYSTEM_PROCESSOR}")

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")

        target_include_directories(${PROJECT_NAME} PUBLIC
            "${CXX_BUILD}/srtc/linux-amd64"
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${CXX_BUILD}/srtc/linux-amd64/librtc.a"
        )

    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")

        target_include_directories(${PROJECT_NAME} PUBLIC
            "${CXX_BUILD}/srtc/linux-arm64"
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${CXX_BUILD}/srtc/linux-arm64/librtc.a"
        )

    else()
        message(FATAL_ERROR "Unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

endif()