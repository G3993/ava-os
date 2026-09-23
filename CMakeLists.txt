cmake_minimum_required(VERSION 3.24)
project(AVAOS LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
  enable_language(OBJC OBJCXX)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "14.4")   # process-tap API floor
endif()

include(FetchContent)

FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.8)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/include.zip)
FetchContent_MakeAvailable(json)

if(WIN32)
  FetchContent_Declare(miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.zip)
  FetchContent_MakeAvailable(miniaudio)
endif()

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC glfw)

set(AVA_COMMON_SRC
  src/main.cpp
  src/audio_mix.cpp
  src/engine.cpp
  src/analysis.cpp
  src/shaderhost.cpp)

if(APPLE)
  set(AVA_ICON "${CMAKE_SOURCE_DIR}/assets/AVA OS.icns")
  set_source_files_properties(${AVA_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  add_executable(AVAOS MACOSX_BUNDLE
    ${AVA_COMMON_SRC}
    ${AVA_ICON}
    src/audio_out.cpp
    src/capture_tap.mm
    src/audio_in.mm
    src/midi_in.mm
    src/midi_out.mm
    src/mac_fullscreen.mm)
  target_compile_definitions(AVAOS PRIVATE GL_SILENCE_DEPRECATION)
  target_link_libraries(AVAOS PRIVATE imgui_lib
    "-framework CoreAudio"
    "-framework CoreMIDI"
    "-framework AudioToolbox"
    "-framework CoreFoundation"
    "-framework Foundation"
    "-framework Accelerate"
    "-framework OpenGL"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo")
  set_target_properties(AVAOS PROPERTIES
    OUTPUT_NAME "AVA OS"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in
    MACOSX_BUNDLE_BUNDLE_NAME "AVA OS"
    MACOSX_BUNDLE_GUI_IDENTIFIER "nyc.soundtemple.os")
  # shader library rides inside the bundle (Contents/Resources/shaders)
  add_custom_command(TARGET AVAOS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/shaders" "$<TARGET_BUNDLE_CONTENT_DIR:AVAOS>/Resources/shaders")
elseif(WIN32)
  add_executable(AVAOS
    ${AVA_COMMON_SRC}
    src/audio_win.cpp
    src/glplat_win.cpp)
  # GL declarations come from src/glplat.h, not the system header
  target_compile_definitions(AVAOS PRIVATE GLFW_INCLUDE_NONE NOMINMAX)
  target_include_directories(AVAOS PRIVATE ${miniaudio_SOURCE_DIR})
  target_link_libraries(AVAOS PRIVATE imgui_lib opengl32 winmm ole32)
  if(MINGW)
    # GUI subsystem (no console window), fully static runtime for portability
    target_link_options(AVAOS PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
  endif()
  set_target_properties(AVAOS PROPERTIES OUTPUT_NAME "AVA OS")
  # shader library ships next to the exe (shaders/)
  add_custom_command(TARGET AVAOS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:AVAOS>/shaders")
endif()

target_include_directories(AVAOS PRIVATE ${json_SOURCE_DIR}/include)
