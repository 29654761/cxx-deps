

include("${CXX_DEPS}/cmake/base.cmake")



SET(ROOT "${CXX_BUILD}/cef")

message("SCTP ABI=${ABI}")
message("SCTP CONFIG=${CONFIG}")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/out/${ABI}-release"    
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libcef.lib"
         "${ROOT}/out/${ABI}-${CONFIG}/lib/libcef_dll_wrapper.lib"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libcef.so"
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libcef_dll_wrapper.a"
    )
endif()
