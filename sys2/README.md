# 介绍
这是最基础的工具类库，大部分其他库都依赖与此

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
add_subdirectory ("${CXX_DEPS}/sys2" "sys2")
```

4. 在你的项目中引用它
```
target_link_libraries(${PROJECT_NAME} PUBLIC
	sys2
)
```
然后这些代码就会参与你的项目一起编译


## 项目依赖
无