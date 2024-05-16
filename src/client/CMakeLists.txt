qt_add_executable(Client
        main.cpp
        resource.qrc
)

target_link_libraries(Client PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
)

set_target_properties(Client PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE ON
)