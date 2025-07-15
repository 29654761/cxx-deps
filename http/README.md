# 介绍
这里封装了HTTP服务器和客户端类型，HTTP服务器支持HTTPS,客户端主要依赖curl

## 使用方法
本库需要CMake编译环境，在要使用这些代码的CMakeList项目中配置
1. 设置两个变量
```
-DCXX_DEPS=/you/cxx-deps/path
-DCXX_BUILD=/you/cxx-build/path
```
2. 可选编译选项
```
HYPERTEXT_CURL = on  # 如果不需要编译客户端，可以关闭此选项
```
3. 包含需要的子项目
```
add_subdirectory ("${CXX_DEPS}/http" "http")
```

4. 在你的项目中引用它
```
target_link_libraries(${PROJECT_NAME} PUBLIC
	http
)
```
然后这些代码就会参与你的项目一起编译


## 项目依赖
- sys2
- hypertext
- asio
- curl.cmake