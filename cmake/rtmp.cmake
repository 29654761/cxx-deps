

include("${CXX_DEPS}/cmake/base.cmake")



SET(ROOT "${CXX_BUILD}/rtmp/rtmp_vs")


target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/rtmp"    
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    if(CMAKE_CL_64)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${ROOT}/x64/${CONFIG}/rtmp.lib"
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${ROOT}/${CONFIG}/rtmp.lib"
        )
    endif()
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/rtmp.a"
    )
endif()
