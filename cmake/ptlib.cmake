

include("${CXX_DEPS}/cmake/base.cmake")
include("${CXX_DEPS}/cmake/openssl.cmake")


SET(ROOT "${CXX_BUILD}/opal")
target_include_directories(${PROJECT_NAME} PUBLIC
    "${ROOT}/ptlib/include"
)


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    if(CMAKE_CL_64)
        target_include_directories(${PROJECT_NAME} PUBLIC
            "${ROOT}/ptlib/lib/x64/include"
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            debug "${ROOT}/ptlib/lib/ptlib64sd.lib"
            optimized "${ROOT}/ptlib/lib/ptlib64s.lib"
        )
    else()

        target_include_directories(${PROJECT_NAME} PUBLIC
            "${ROOT}/ptlib/lib/win32/include"
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            debug "${ROOT}/ptlib/lib/ptlibsd.lib"
            optimized "${ROOT}/ptlib/lib/ptlibs.lib"
        )
    endif()
else()
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/ptlib/lib/include"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        debug "${ROOT}/ptlib/lib/libpt_s.a"
        optimized "${ROOT}/ptlib/lib/libpt_s.a"
        resolv
        rt
	    dl
    )
endif()
