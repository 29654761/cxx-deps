# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

C++ monorepo providing reusable components for real-time communications (WebRTC, SIP/H.323, RTP, RTSP, HTTP). Each directory is a self-contained module compiled as a static library and intended to be consumed via `add_subdirectory()` in downstream projects.

## Build System

This repo is consumed as a dependency — it does not build standalone. Downstream projects must provide two CMake cache variables:

```cmake
-DCXX_DEPS=/path/to/cxx-deps   # this repo
-DCXX_BUILD=/path/to/cxx-build  # prebuilt third-party binaries
```

Third-party binaries follow the layout `${CXX_BUILD}/<lib>/out/<ABI>-<CONFIG>/{include,lib}/`. ABI is detected in `cmake/base.cmake` (x64/win32/linux/wasm). CONFIG is debug or release.

To build a single module in isolation:

```bash
mkdir build && cd build
cmake .. -DCXX_DEPS=E:/cxx-deps -DCXX_BUILD=E:/cxx-build -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Module Architecture

Dependencies flow in layers — never circle back up:

```
sfu, any            ← top-level consumers
voip                ← sip + litertp (migration to rtpx planned)
rtpx, http, rtsp, mqtt
sip, litertp        ← sip primitives; litertp is deprecated
ssl, asio, hypertext, ffmpeg
sys2                ← foundation, no deps
spdlog              ← header-only logging, used everywhere
```

### Key modules

| Module | Purpose | C++ std | Notes |
|--------|---------|---------|-------|
| **sys2** | File system, threading, XML (pugixml), hardware info | C++17 | Start here — all others depend on it |
| **rtpx** | Modern RTP/WebRTC stack (H.264/H.265/VP8/VP9/AV1/opus/AAC, SRTP, DTLS, SCTP, RTX) | C++17 | Prefer over litertp |
| **sfu** | WebRTC Selective Forwarding Unit | C++20 | Wraps rtpx; recent lock-free subscription fixes |
| **voip** | SIP + H.323 calling (DTMF RFC 4733) | C++14 (Win) / C++17 | Still uses deprecated litertp |
| **http** | HTTP/HTTPS server + curl-based client | C++17 | Enable curl client with `-DHYPERTEXT_CURL=ON` |
| **litertp** | Legacy RTP library | C++17 | Deprecated — only voip still uses it |

### Optional compile-time features

- `-DRTPX_SSL=ON` — SRTP/DTLS support (requires openssl + srtp cmake configs)
- `-DLITERALTP_SCTP=ON` — SCTP data channels
- `-DHYPERTEXT_CURL=ON/OFF` — curl HTTP client in http module (default ON)

## Platform-Specific Code

Platform variants live alongside generic code:

- `*_win.cpp` / `*_linux.cpp` — included via CMake per platform
- Windows requires `-DNOMINMAX` (set globally in all module CMakeLists)
- Android NDK and Emscripten (wasm) are supported targets

## rtpx Usage Pattern

```cpp
auto* core = rtpx::rtpx_core::instance();
core->init(worker_thread_count);  // typically CPU_COUNT * 2 + 2

auto session = core->create_rtp_session(/*send=*/true, /*recv=*/true);
session->on_frame = [](const std::string& mid, const rtpx::sdp_format& fmt,
                       const av_frame_t& frame) { /* handle decoded media */ };
```

## Repository Remotes

- Primary: `https://code.zaoing.com/rtc/cxx-deps`
- Mirror: `https://github.com/29654761/cxx-deps.git`

Documentation (README, CMake messages) is written in Chinese.
