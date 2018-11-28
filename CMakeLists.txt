cmake_minimum_required(VERSION 3.1.0)
project(zsfcgi)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_search_module(cgicc REQUIRED cgicc)

include_directories(${cgicc_INCLUDE_DIRS})
add_executable(zsfcgi app.cxx main.cxx util.cxx 3rdparty/FCgiIO.cpp)
target_link_libraries(zsfcgi fcgi fcgi++ ${cgicc_LIBRARIES} pcre pcrecpp)
