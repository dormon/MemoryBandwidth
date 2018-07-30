cmake_minimum_required(VERSION 3.10.1)

project(MemoryBandwidth)

SET(CMAKE_CXX_STANDARD 14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(MAJOR_VERSION 1)
set(MINOR_VERSION 0)
set(REVISION_VERSION 0)

find_package(ArgumentViewer 1.0 CONFIG REQUIRED)
find_package(geGL 1.0.4 CONFIG REQUIRED)
find_package(SDL2CPP 1.0 CONFIG REQUIRED)
find_package(imguiSDL2OpenGL CONFIG REQUIRED)
find_package(Vars CONFIG REQUIRED)

set(SOURCES
  main.cpp
  )

set(INCLUDES
  )

add_executable(${PROJECT_NAME} ${SOURCES} ${INCLUDES})

target_include_directories(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

target_link_libraries(${PROJECT_NAME} PUBLIC SDL2CPP::SDL2CPP geGL::geGL ArgumentViewer::ArgumentViewer imguiSDL2OpenGL::imguiSDL2OpenGL Vars::Vars)

