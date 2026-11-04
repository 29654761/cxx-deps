# 介绍
这是一个轻量的RTP协议库，支持90%的webrtc功能和大部分SIP,H.323,RTSP等协议的交互，支持丢包重传(直接重传或RTX重传)，I帧请求等基本的弱网策略。本库只包含传输，不包含编解码库。

## 使用方法
本库需要CMake编译环境，在要使用这些代码的CMakeList项目中配置
1. 设置两个变量
```
-DCXX_DEPS=/you/cxx-deps/path
-DCXX_BUILD=/you/cxx-build/path
```
2. 可选编译选项
```
RTPX_SSL = on  # 如果需要支持srtp 则打开此选项

```
3. 包含需要的子项目
```
add_subdirectory ("${CXX_DEPS}/rtpx" "rtpx")
```

4. 在你的项目中引用它
```
target_link_libraries(${PROJECT_NAME} PUBLIC
	rtpx
)
```
然后这些代码就会参与你的项目一起编译


## 项目依赖
- sys2
- asio
- openssl.cmake
- srtp.cmake

## 接口调用
### 全局初始化
初始化
``` C++
auto* rtpx = rtpx::rtpx_core::instance();
rtpx->set_port_range(port_begin, port_end); //可选，设置RTP端口范围
rtpx->init(cpu * 2 + 2); //初始化worker线程数
```
注销
``` C++
rtpx::rtpx_core::release();
```

### 创建RTP Session (同PeerConnection)
``` C++
auto rtpx=rtpx::rtpx_core::instance();
auto session_ = rtpx->create_rtp_session(true,true);
//绑定事件
session_->on_connected=[this]() {}; //连接成功，只有DTLS握手才会触发此事件
session_->on_disconnected= [this](){}; //断开连接
session_->on_frame= [this](const std::string& mid, const rtpx::sdp_format& fmt, const av_frame_t& frame){}//收到媒体数据
session_->on_receive_require_keyframe = [this](const std::string& mid){} //收到I帧请求
session_->on_data_channel_opened = [this](const std::string& mid){} //DataChannel打开
session_->on_data_channel_closed = [this](const std::string& mid){} //DataChannel关闭
session_->on_data_channel_message= [this](const std::string& mid,const std::vector<uint8_t>& message){} //收到DataChannel消息
```
### 媒体能力集（全局支持的媒体格式）
本端支持的编解码格式通过全局能力集统一描述（RFC 3264）：
- **Offer**：从能力集生成本端格式（含 fmtp、rtcp-fb），pt 使用注册时的建议值；
- **Answer**：`set_remote_sdp(offer)` 内部自动按能力集匹配，只保留对端 offer 与能力集的交集，pt 沿用对端编号；
- 格式匹配规则：codec、采样率、packetization-mode、profile、level（见 `sdp_format::is_compatible_with`）；
- 未注册任何能力时保持旧行为（镜像对端 rtpmap），不影响既有调用方。

注册能力集（进程内一次即可，应在创建 RTP Session 之前完成）：
``` C++
auto& caps = rtpx::media_capabilities::global();
caps.clear();

rtpx::sdp_format fmt(8, codec_type_pcma, 8000);
caps.add_format(media_type_audio, fmt);

fmt = rtpx::sdp_format(96, codec_type_h264, 90000);
fmt.fmtp.set_packetization_mode(1);
fmt.fmtp.set_profile_level_id(0x420028);
fmt.fmtp.set_level_asymmetry_allowed(1);
caps.add_format(media_type_video, fmt);
```

创建媒体流后从能力集生成本端格式：
``` C++
auto ms = session_->create_media_stream(media_type_video, "0", true, ms_sid, ms_tid, rtp_trans_mode_sendrecv);
for (auto& fmt : rtpx::media_capabilities::global().formats(media_type_video))
{
    ms->add_local_format(fmt);
}
```

### Offer
视频
``` C++
auto ms=session_->create_media_stream(media_type_audio,"0", true, ms_sid, ms_tid, rtp_trans_mode_sendrecv);
ms->add_local_video_track(codec_type_h264,106,90000,true);
ms->add_local_rtx_track(107,106, 90000, true);
rtpx::fmtp fmtp;
fmtp.set_level_asymmetry_allowed(1);
fmtp.set_packetization_mode(1);
fmtp.set_profile_level_id(0x42e01f);
ms->set_local_rtpmap_fmtp(106, fmtp);

ms->add_local_video_track(codec_type_vp8,108,90000,true);
ms->add_local_rtx_track(109,108, 90000, true);

rtpx::sdp offer_sdp
session_->create_offer(offer_sdp);

//Get answer sdp

session_->set_remote_sdp(answer_sdp,sdp_type_answer);
```


### Answer
``` C++
//Get offer sdp
session_->set_remote_sdp(offer_sdp,sdp_type_offer);

rtpx::sdp answer_sdp
session_->create_answer(answer_sdp);

//Send answer sdp
```
### 发送数据

``` C++
auto ms = session_->get_media_stream_by_mid(mid);
if(ms)
{
	ms->send_frame(106,frame, size, duration);  //Send H.264 frame
	ms->send_frame(108,frame,size,duration);   //Send VP8 frame
}

```

### 其他
对于无Offer/Answer交互的协议，可以调用 ms->set_local_xxx和ms->set_remote_xxx来构建两端信息

### 流用途标记（content/label）
`ms->set_content("slides")` / `ms->set_label("12")`（RFC 4796 / RFC 4574）用于标记流用途，
生成 offer 时输出为 `a=content:slides` / `a=label:12`；处理对端 SDP 时自动捕获对端 m 行的
取值，生成 answer 时原样回带（典型场景：辅流视频标记 + 与 BFCP `a=floorid:2 mstrm:12` 关联）
###