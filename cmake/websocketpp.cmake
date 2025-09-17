

include("${CXX_DEPS}/cmake/base.cmake")

add_definitions(-DASIO_STANDALONE -D_WEBSOCKETPP_CPP11_STL_)



include_directories(
    "${CXX_BUILD}/websocketpp"    
)


