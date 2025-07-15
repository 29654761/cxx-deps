

include("${CXX_DEPS}/cmake/base.cmake")



SET(ROOT "${CXX_BUILD}/libsrtp")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/out/${ABI}-${CONFIG}/include"    
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/srtp2.lib"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libsrtp2.a"
    )
endif()
