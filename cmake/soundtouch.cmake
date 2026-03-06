

include("${CXX_DEPS}/cmake/base.cmake")



target_include_directories(${PROJECT_NAME} PRIVATE
    "${CXX_BUILD}/soundtouch/out/${ABI}-${CONFIG}/include"
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
target_link_libraries(${PROJECT_NAME} PRIVATE
    "${CXX_BUILD}/soundtouch/out/${ABI}-${CONFIG}/lib/SoundTouch.lib"
)
else()
target_link_libraries(${PROJECT_NAME} PRIVATE
    "${CXX_BUILD}/soundtouch/out/${ABI}-${CONFIG}/lib/libSoundTouch.a"
)
endif()
