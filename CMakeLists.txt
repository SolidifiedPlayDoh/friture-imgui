cmake_minimum_required(VERSION 3.20)
project(friture_imgui VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ---- GLFW ---------------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE)

# ---- Dear ImGui (docking branch) + ImPlot --------------------------------
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
  GIT_SHALLOW TRUE)
FetchContent_Declare(implot
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG master
  GIT_SHALLOW TRUE)

# ---- miniaudio (audio I/O) and pocketfft (header-only FFT) ----------------
FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 0.11.22
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL)
FetchContent_Declare(pocketfft
  GIT_REPOSITORY https://github.com/mreineck/pocketfft.git
  GIT_TAG cpp
  GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw imgui implot miniaudio pocketfft)

find_package(OpenGL REQUIRED)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
  ${implot_SOURCE_DIR}/implot_demo.cpp)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${implot_SOURCE_DIR})
target_compile_definitions(imgui_lib PUBLIC
  IMGUI_DISABLE_OBSOLETE_FUNCTIONS GL_SILENCE_DEPRECATION)
target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

file(GLOB_RECURSE FRITURE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_executable(friture ${FRITURE_SOURCES})
target_include_directories(friture PRIVATE
  ${CMAKE_SOURCE_DIR}/src ${miniaudio_SOURCE_DIR} ${pocketfft_SOURCE_DIR})
target_link_libraries(friture PRIVATE imgui_lib)
target_compile_definitions(friture PRIVATE
  FRITURE_FONT_PATH="${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf"
  FRITURE_VERSION="${PROJECT_VERSION}")

if(APPLE)
  target_link_libraries(friture PRIVATE
    "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(friture PRIVATE -Wall -Wextra -Wno-unused-parameter)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(friture PRIVATE -O3)
  endif()
endif()
