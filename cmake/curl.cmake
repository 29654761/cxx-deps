

include("${CXX_DEPS}/cmake/base.cmake")

add_definitions(-DBUILDING_LIBCURL -DHTTP_ONLY)


message("CURL path for ${PROJECT_NAME}: ${CXX_BUILD}/curl/out/${ABI}-${CONFIG}")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CXX_BUILD}/curl/out/${ABI}-${CONFIG}/include"    
)


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        debug "${CXX_BUILD}/curl/out/${ABI}-${CONFIG}/lib/libcurl-d.lib"
        optimized "${CXX_BUILD}/curl/out/${ABI}-${CONFIG}/lib/libcurl.lib"
	debug "${CXX_BUILD}/c-areas/out/${ABI}-${CONFIG}/lib/cares_static.lib"
        optimized "${CXX_BUILD}/c-areas/out/${ABI}-${CONFIG}/lib/cares_static.lib"
        ws2_32.lib
        wldap32.lib
        winmm.lib
        Crypt32.lib
        Normaliz.lib
        debug msvcrtd.lib
        optimized msvcrt.lib
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        debug "${CXX_BUILD}/curl/out/${ABI}-${CONFIG}/lib/libcurl-d.a"
        optimized "${CXX_BUILD}/curl/out/${ABI}-${CONFIG}/lib/libcurl.a"
	debug "${CXX_BUILD}/c-areas/out/${ABI}-${CONFIG}/lib/libcares.a"
        optimized "${CXX_BUILD}/c-areas/out/${ABI}-${CONFIG}/lib/libcares.a"
    )
endif()


include("${CXX_DEPS}/cmake/zlib.cmake")
include("${CXX_DEPS}/cmake/openssl.cmake")
