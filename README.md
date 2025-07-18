# cxx-deps
这是一个C++工具类代码库，是我自己在工作中积累的，其中有很多有用的实现，如果对你也有帮助，尽管拿去用。
具体可以进入每个模块查看文档。

## 使用方法
本库需要CMake编译环境，在要使用这些代码的CMakeList项目中配置
1. 设置两个变量
```
-DCXX_DEPS=/you/cxx-deps/path
-DCXX_BUILD=/you/cxx-build/path
```

2. 设置头文件目录
```
include_directories(${CXX_DEPS})
```
3. 包含需要的子项目
```
add_subdirectory ("${CXX_DEPS}/sys2" "sys2")
add_subdirectory ("${CXX_DEPS}/ssl" "http")
add_subdirectory ("${CXX_DEPS}/rtpx" "rtpx")
```

4. 在你的项目中引用它
```
target_link_libraries(${PROJECT_NAME} PUBLIC
	sys2
	http
	rtpx
)
```
然后这些代码就会参与你的项目一起编译

## CXX-BUILD
这是其他第三方库，需要按照以下目录格式进行编译输出
以CURL为例：
```
${CXX_BUILD}/curl/out/${ABI}-${CONFIG}/include
${CXX_BUILD}/curl/out/${ABI}-${CONFIG}/lib
```
这些配置都在`${CXX_DEPS}/cmake/curl.cmake` 中，可以根据自己实际情况修改。
如果你全部修改了${CXX_DEPS}/cmake里的配置文件，也可以不需要CXX_BUILD变量

