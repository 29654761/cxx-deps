

include("${CXX_DEPS}/cmake/base.cmake")



SET(ROOT "${CXX_BUILD}/jsoncpp")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/out/${ABI}-${CONFIG}/include"    
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/jsoncpp_static.lib"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libjsoncpp.a"
    )
endif()
