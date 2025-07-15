

include("${CXX_DEPS}/cmake/base.cmake")



SET(ROOT "${CXX_BUILD}/sctp")

message("SCTP ABI=${ABI}")
message("SCTP CONFIG=${CONFIG}")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/out/${ABI}-${CONFIG}/include"    
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/usrsctp.lib"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${ROOT}/out/${ABI}-${CONFIG}/lib/libusrsctp.a"
    )
endif()
