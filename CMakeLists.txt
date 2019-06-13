cmake_minimum_required(VERSION 3.0.2)
project(escada_core)

set(CMAKE_CXX_STANDARD 11)
add_definitions(-std=c++11)

include_directories(.)
include_directories(version)

# TODO: сделать проверку на тип системы на которой производится сборка

include_directories (/usr/local/mysql/include/)
include_directories (/usr/local/Cellar/glib/2.58.3/lib/glib-2.0/include)
include_directories (/usr/local/Cellar/libgtop/2.38.0_1/include/libgtop-2.0)
include_directories (/usr/local/include/glib-2.0)
include_directories (/usr/include/glib-2.0)

#set(CMAKE_C_FLAGS "-m32")
#set(CMAKE_CXX_FLAGS "-m32")

# TODO: сделать проверку на то под какую архитектуру собираем, от этого подключать нужную папку
include_directories(/usr/lib/x86_64-linux-gnu/glib-2.0/include)
include_directories (/usr/lib/i386-linux-gnu/glib-2.0/include)
include_directories (/usr/include/libgtop-2.0)

include_directories (/usr/local/include/nettle)
include_directories(/usr/local/lib)

set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib)

add_executable(escada_core
        consolecolors.h
        const.h
        dbase.cpp
        dbase.h
        errors.h
        kernel.cpp
        kernel.h
        logs.cpp
        logs.h
        drivers/mercury230.cpp
        drivers/mercury230.h
        drivers/ce303.cpp
        drivers/ce303.h
        tinyxml2.cpp
        tinyxml2.h TypeThread.cpp TypeThread.h drivers/MtmZigbee.cpp drivers/MtmZigbee.h function.cpp)

target_link_libraries(escada_core mysqlclient)
target_link_libraries(escada_core pthread)
target_link_libraries(escada_core gtop-2.0)
target_link_libraries(escada_core nettle)
target_link_libraries(escada_core uuid)

include_directories(external/zigbeemtm)
add_subdirectory(external/zigbeemtm)
target_link_libraries(escada_core zigbeemtm)

set_target_properties(escada_core PROPERTIES RUNTIME_OUTPUT_DIRECTORY ./)