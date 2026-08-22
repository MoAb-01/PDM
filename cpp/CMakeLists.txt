cmake_minimum_required(VERSION 3.20)
project(DownloadManagerAB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE)

include_directories(include)

add_executable(DownloadManagerAB WIN32
    src/MainWindow.cpp
    src/DownloadEngine.cpp
    src/FilePropertiesDialog.cpp
    src/OptionsDialog.cpp
    src/ChunkVisualizerDialog.cpp
    src/DownloadProgressDialog.cpp
    src/AddUrlDialog.cpp
    src/SchedulerDialog.cpp
    src/DownloadFileInfoDialog.cpp
    src/DownloadCompleteDialog.cpp
)

target_link_libraries(DownloadManagerAB PRIVATE
    comctl32
    wininet
    winhttp
    ws2_32
    dwmapi
    uxtheme
    shlwapi
    user32
    gdi32
    gdiplus
    d2d1
    dwrite
    shell32
)

add_executable(IDMNativeHost
    src/IDMNativeHost.cpp
)
