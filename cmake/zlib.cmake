

include("${CXX_DEPS}/cmake/base.cmake")

message("ZLIB path for ${PROJECT_NAME}:${CXX_BUILD}/zlib/out/${ABI}-${CONFIG}")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CXX_BUILD}/zlib/out/${ABI}-${CONFIG}/include"    
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        debug "${CXX_BUILD}/zlib/out/${ABI}-${CONFIG}/lib/zlibstaticd.lib"
        optimized "${CXX_BUILD}/zlib/out/${ABI}-${CONFIG}/lib/zlibstatic.lib"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${CXX_BUILD}/zlib/out/${ABI}-${CONFIG}/lib/libz.a"
    )
endif()
