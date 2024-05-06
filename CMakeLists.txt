cmake_minimum_required(VERSION 3.27)
project(AIMS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.6.3/mingw_64/lib/cmake/")

find_package(Qt6 COMPONENTS
        Gui
        Core
        Sql
        HttpServer
        REQUIRED)

add_subdirectory(src/server)
add_subdirectory(src/client)