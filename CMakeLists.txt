cmake_minimum_required(VERSION 3.27)
project(AIMS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.6.3/mingw_64/lib/cmake/")
set(OPENSSL_ROOT_DIR "C:/Qt/Tools/OpenSSL/Win_x64/")

find_package(Qt6 COMPONENTS
        Gui
        Core
        Sql
        HttpServer
        Widgets
        Network
        REQUIRED)

add_subdirectory(jwt-cpp)
add_subdirectory(src/server)
add_subdirectory(src/client)