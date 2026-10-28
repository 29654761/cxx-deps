# 介绍
这是一个支持SIP和H.323协议的VOIP库,支持几乎所有涉及到的功能(BFCP待实现)

## 使用方法
本库需要CMake编译环境，在要使用这些代码的CMakeList项目中配置
1. 设置两个变量
```
-DCXX_DEPS=/you/cxx-deps/path
-DCXX_BUILD=/you/cxx-build/path
```
2. 可选编译选项
```
无
```
3. 包含需要的子项目
```
add_subdirectory ("${CXX_DEPS}/voip" "voip")
```

4. 在你的项目中引用它
```
target_link_libraries(${PROJECT_NAME} PUBLIC
	voip
)
```
然后这些代码就会参与你的项目一起编译


## 项目依赖
- sys2
- rtpx (RTP 传输，取代已废弃的 litertp)
- opal.cmake

## 从 litertp 版本迁移说明
- `endpoint::set_rtp_ports`（H.323 还有 `set_h245_ports`）的参数类型由 `litertp::port_range_ptr` 改为 `rtpx::port_range_ptr`，接口形式不变
- RTP 收发绑定 endpoint 自己的 `asio::io_context`，无需初始化 rtpx_core

## 媒体能力集
本端支持的编解码格式由全局能力集（`rtpx::media_capabilities::global()`）描述：
- 首次创建 call 时自动注册默认能力集：PCMA/PCMU/telephone-event(101) + H264(96/420028、98/640028) + H265(100)；
- 主叫 offer 的 rtpmap 从能力集生成，被叫应答只保留对端 offer 与能力集的交集（pt 沿用对端编号）；
- 如需调整（例如 offer 不要 H265），在创建 endpoint 之前修改全局能力集即可：
```C++
auto& caps = rtpx::media_capabilities::global();
caps.clear();   // 清空后按需重新注册，格式见 rtpx/README.md 媒体能力集章节
```