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
- litertp (该项目已废弃，后面会改成对rtpx的引用)
- opal.cmake