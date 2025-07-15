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