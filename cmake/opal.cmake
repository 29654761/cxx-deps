

include("${CXX_DEPS}/cmake/base.cmake")
include("${CXX_DEPS}/cmake/openssl.cmake")


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    SET(ROOT "${CXX_BUILD}/opal")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/opal/include"
        "${ROOT}/ptlib/include"
    )
else()
    SET(ROOT "${CXX_BUILD}/opal/out/${ABI}-${CONFIG}")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/opal/include/opal"
        "${ROOT}/ptlib/include"
    )
endif()





if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    if(CMAKE_CL_64)
        target_include_directories(${PROJECT_NAME} PUBLIC
            "${ROOT}/opal/lib/x64/include"
            "${ROOT}/ptlib/lib/x64/include"
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            debug "${ROOT}/opal/lib/opal64sd.lib"
            optimized "${ROOT}/opal/lib/opal64s.lib"
            debug "${ROOT}/ptlib/lib/ptlib64sd.lib"
            optimized "${ROOT}/ptlib/lib/ptlib64s.lib"
        )
    else()

        target_include_directories(${PROJECT_NAME} PUBLIC
            "${ROOT}/opal/lib/win32/include"
            "${ROOT}/ptlib/lib/win32/include"
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            debug "${ROOT}/opal/lib/opalsd.lib"
            optimized "${ROOT}/opal/lib/opals.lib"
            debug "${ROOT}/ptlib/lib/ptlibsd.lib"
            optimized "${ROOT}/ptlib/lib/ptlibs.lib"
        )
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/opal/lib/include"
        "${ROOT}/ptlib/lib/include"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        debug "${ROOT}/opal/lib/libopal_s.a"
        optimized "${ROOT}/opal/lib/libopal_s.a"
        debug "${ROOT}/ptlib/lib/libpt_s.a"
        optimized "${ROOT}/ptlib/lib/libpt_s.a"
	    dl
    )
else()
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/opal/lib/include"
        "${ROOT}/ptlib/lib/include"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        debug "${ROOT}/opal/lib/libopal_s.a"
        optimized "${ROOT}/opal/lib/libopal_s.a"
        debug "${ROOT}/ptlib/lib/libpt_s.a"
        optimized "${ROOT}/ptlib/lib/libpt_s.a"
        resolv
        rt
	    dl
    )
endif()
