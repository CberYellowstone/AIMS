qt_add_executable(Server
        main.cpp
        ../client/mainapp.ui
        database.cpp
        database.h
)

target_link_libraries(Server PRIVATE
        Qt::Core
        Qt::Sql
        Qt::HttpServer
)

set_target_properties(Server PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE ON
)

#add_definitions(-DQT_NO_DEBUG_OUTPUT)