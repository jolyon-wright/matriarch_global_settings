cmake_minimum_required(VERSION 3.21.2)

project(matriarch_global_settings VERSION 0.0.1)

if (MSVC)
  add_compile_options(/W4 /WX-)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

find_package(fmt REQUIRED)
find_package(RtMidi CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

file(GLOB SRC CONFIGURE_DEPENDS "*.h" "*.cpp")

include(GNUInstallDirs)
include_directories(${CMAKE_INSTALL_INCLUDEDIR})

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  RtMidi::rtmidi
  spdlog::spdlog spdlog::spdlog_header_only
  # PUBLIC
  )

