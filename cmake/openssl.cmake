

include("${CXX_DEPS}/cmake/base.cmake")



SET(ROOT "${CXX_BUILD}/openssl")

message("OPENSSL path for ${PROJECT_NAME}: ${ROOT}/out/${ABI}-${CONFIG}")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/out/${ABI}-${CONFIG}/include"    
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libssl.lib"
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libcrypto.lib"
        ws2_32
        Crypt32
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libssl.a"
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libcrypto.a"
    )
endif()
