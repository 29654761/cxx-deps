# 介绍
这是一个支持SIP和H.323协议的VOIP库,支持几乎所有涉及到的功能（含 BFCP 辅流/RFC 4582，H.224 未实现）

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

## 辅流（BFCP）
SIP 侧辅流通过 BFCP（RFC 4582，UDP/BFCP）实现，与 H.323 侧的 H.239 接口一致：
- `call::request_presentation_role()` —— 发送 FloorRequest 请求辅流令牌
- `call::release_presentation_role()` —— 发送 FloorRelease 释放令牌
- `call::has_presentation_role()` —— 令牌持有状态
- `call::set_on_presentation_role_changed(call, owned, active)` —— 授予/拒绝/释放/收回时回调
- 辅流视频：授予后通过 `call::send_video_frame(data, size, dur, is_extend=true)` 发送（mid=extend_video）
- 主叫需开启 `sip::endpoint::set_content_support(true)`，offer 携带第二条 video（mid=extend_video，
  含 `a=content:slides` 与 `a=label:12`，RFC 4796/4574）与 `m=application UDP/BFCP *`
  （含 a=confid/userid/floorctrl 与 `a=floorid:2 mstrm:12`，mstrm 与视频行的 label 对应，RFC 4583）；
  被叫按对端 offer 自适应协商，应答回带对端的 content/label 与 floorid/mstrm
- H.323 侧对应开关为 `h323::endpoint::set_content_support(true)`：开启后 TCS 能力集附加
  extend_video 通道与 h239 控制通道；主/被叫双方都需开启才能使用辅流
- 端口规划：开启辅流后每个视频呼叫需要 7 个端口（audio/video/extend_video 三对 + BFCP 一个），
  BFCP 从 `set_rtp_ports` 的同一范围分配且绝不复用媒体端口；范围不足时辅流自动禁用
  （日志 `BFCP: bind local port failed, rtp ports ... exhausted`），建议范围 ≥ 8
- 收到对端 FloorRequest 时本端作为 chair 决策：未持有令牌则授予（Granted），已持有则拒绝（Denied）
- 对端 offer 中的 H.224（`m=application RTP/AVP 100`）暂不支持，按 RFC 3264 以端口 0 拒绝