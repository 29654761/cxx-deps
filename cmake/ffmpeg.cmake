

include("${CXX_DEPS}/cmake/base.cmake")


if(CMAKE_SYSTEM_NAME MATCHES "Windows")

    SET(ROOT "${CXX_BUILD}/ffmpeg_bin/ffmpeg.6.1.1")
    message("FFmpeg path for ${PROJECT_NAME}: ${ROOT}/win/lib/win-${ABI}-${CONFIG}")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/include"    
    )

    target_link_libraries(${PROJECT_NAME} PUBLIC
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/lib/avdevice-60.lib"
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/lib/avfilter-9.lib"
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/lib/avformat-60.lib"
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/lib/avcodec-60.lib"
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/lib/avutil-58.lib"
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/lib/postproc-57.lib"
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/lib/swresample-4.lib"
        "${ROOT}/win/lib/win-${ABI}-${CONFIG}/lib/swscale-7.lib"
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(ROOT "${CXX_BUILD}/ffmpeg_bin/ffmpeg.6.1.1")
    message("FFmpeg path for ${PROJECT_NAME}: ${ROOT}/${ABI}/")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/${ABI}/include"    
    )
    target_link_libraries(${PROJECT_NAME} PUBLIC
        "${ROOT}/${ABI}/lib/libavcodec.so.60"
        "${ROOT}/${ABI}/lib/libavdevice.so.60"
        "${ROOT}/${ABI}/lib/libavfilter.so.9"
        "${ROOT}/${ABI}/lib/libavformat.so.60"
        "${ROOT}/${ABI}/lib/libavutil.so.58"
        "${ROOT}/${ABI}/lib/libpostproc.so.57"
        "${ROOT}/${ABI}/lib/libswresample.so.4"
        "${ROOT}/${ABI}/lib/libswscale.so.7"
	pthread
	dl
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    SET(ROOT "${CXX_BUILD}/ffmpeg_bin/ffmpeg.6.1.1")
    message("FFmpeg path for ${PROJECT_NAME}: ${ROOT}/${ABI}/")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/${ABI}/include"    
    )
    file(GLOB_RECURSE LIBS "${ROOT}/${ABI}/lib/*.a")
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${LIBS}
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
    SET(ROOT "${CXX_BUILD}/ffmpeg/ffmpeg/out")
    message("FFmpeg path for ${PROJECT_NAME}: ${ROOT}/${ABI}/")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${ROOT}/${ABI}/include"    
    )
    target_link_libraries(${PROJECT_NAME} PUBLIC
        "${ROOT}/${ABI}/lib/libavcodec.so"
        "${ROOT}/${ABI}/lib/libavdevice.so"
        "${ROOT}/${ABI}/lib/libavfilter.so"
        "${ROOT}/${ABI}/lib/libavformat.so"
        "${ROOT}/${ABI}/lib/libavutil.so"
        "${ROOT}/${ABI}/lib/libpostproc.so"
        "${ROOT}/${ABI}/lib/libswresample.so"
        "${ROOT}/${ABI}/lib/libswscale.so"
	    dl
    )

endif()
