

include("${CXX_DEPS}/cmake/base.cmake")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
if(MSVC)
        # Windows / MSVC
        message(STATUS "→ Configuring MSVC: /O2 /Zi /DEBUG /OPT:REF /OPT:ICF")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Zi")
        set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /O2 /Zi")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE
            "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Windows / MinGW or Clang
        message(STATUS "→ Configuring MinGW/Clang: -O2 -g")
        set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")
        set(CMAKE_C_FLAGS_RELEASE   "-O2 -g")

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(STATUS "→ Configuring Linux: -O2 -g -fno-omit-frame-pointer")
        set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g -fno-omit-frame-pointer")
        set(CMAKE_C_FLAGS_RELEASE   "-O2 -g -fno-omit-frame-pointer")

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
        message(STATUS "→ Configuring Android: -O2 -g")
        set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")
        set(CMAKE_C_FLAGS_RELEASE   "-O2 -g")

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # macOS / iOS
        message(STATUS "→ Configuring macOS/iOS: -O2 -g -fno-omit-frame-pointer -Wl,-S")
        set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g -fno-omit-frame-pointer")
        set(CMAKE_C_FLAGS_RELEASE   "-O2 -g -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE
            "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-S")

    else()
        message(WARNING "→ Unknown platform, using generic -O2 -g")
        set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")
        set(CMAKE_C_FLAGS_RELEASE   "-O2 -g")
    endif()
endif()

