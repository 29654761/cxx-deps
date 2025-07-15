# CMakeList.txt: vcs-streamserver 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

# 将源代码添加到此项目的可执行文件。

project ("any")



file(GLOB_RECURSE SRCS *.cpp *.c *.cc)

file(GLOB_RECURSE INC "inc/*.*")
list(REMOVE_ITEM SRCS ${INC})

add_library (${PROJECT_NAME} STATIC ${SRCS} )



# TODO: 如有需要，请添加测试并安装目标。

target_include_directories(${PROJECT_NAME} PUBLIC
    .
    ./inc
)

 target_link_libraries(${PROJECT_NAME} PRIVATE
    sys2
    ${LIB_ANYCLIENT}
 )


