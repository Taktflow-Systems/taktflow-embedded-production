cmake_minimum_required(VERSION 3.20)

project(arxml_editor VERSION 0.1 LANGUAGES CXX)

# Force AUTOMOC
set(CMAKE_AUTOMOC ON)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 modules
find_package(Qt6 6.9 COMPONENTS Widgets Xml Gui REQUIRED)
qt_standard_project_setup()

qt_add_executable(arxml_editor
    src/main.cpp
    src/main_window.cpp
    src/arxml_model.cpp
    src/arxml_validator.cpp

    inc/main_window.hpp
)

# Include the source folder so the header can be found
target_include_directories(arxml_editor PRIVATE ${CMAKE_SOURCE_DIR}/inc)

# Link against Qt6 Widgets and Xml
target_link_libraries(arxml_editor PRIVATE Qt6::Widgets Qt6::Xml)
