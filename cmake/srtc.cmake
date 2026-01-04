

include("${CXX_DEPS}/cmake/base.cmake")





if(CMAKE_SYSTEM_NAME MATCHES "Windows")

    target_include_directories(${PROJECT_NAME} PUBLIC
        "${CXX_BUILD}/srtc/windows-amd64"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
       "${CXX_BUILD}/srtc/windows-amd64/librtc.lib"
    )
else()
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${CXX_BUILD}/srtc/linux-amd64"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
       "${CXX_BUILD}/srtc/linux-amd64/librtc.a"
    )
endif()

