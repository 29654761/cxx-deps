

include("${CXX_DEPS}/cmake/base.cmake")



target_include_directories(${PROJECT_NAME} PUBLIC
    "${CXX_BUILD}/mmkv/out/${ABI}-${CONFIG}/include"
)



if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
       "${CXX_BUILD}/mmkv/out/${ABI}-${CONFIG}/lib/core.lib"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
         "${CXX_BUILD}/mmkv/out/${ABI}-${CONFIG}/lib/libcore.a"
    )
endif()

