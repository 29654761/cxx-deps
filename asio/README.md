# 介绍
这是仅包含头文件的ASIO库，我只增加了tcp和udp两个socket的类，实现了队列化发送

## 使用方法
本库需要CMake编译环境，在要使用这些代码的CMakeList项目中配置
1. 设置两个变量
```
-DCXX_DEPS=/you/cxx-deps/path
-DCXX_BUILD=/you/cxx-build/path
```
2. 包含需要的子项目
```
add_subdirectory ("${CXX_DEPS}/asio" "asio")
```

3. 在你的项目中引用它
```
target_link_libraries(${PROJECT_NAME} PUBLIC
	asio
)
```
然后这些代码就会参与你的项目一起编译


## 项目依赖
无