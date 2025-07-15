
message("CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    if(CMAKE_CL_64)
        SET(ABI "x64")
    else()
        SET(ABI "win32")
    endif()

elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(ABI "linux")

endif()

string(TOLOWER ${CMAKE_BUILD_TYPE} CONFIG)





