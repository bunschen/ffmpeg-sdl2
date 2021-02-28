cmake_minimum_required(VERSION 3.12)
project(sdl_test)

set(CMAKE_CXX_STANDARD 14)
set(SDL_DIR /usr/local/Cellar/sdl2/2.0.14_1) # 声明一个变量SDL2_DIR为SDL2安装路径, SDL开发包的路径，可以通过brew info sdl2查出来
set(FFMPEG_DIR /usr/local/Cellar/ffmpeg/4.3.1_6) # 声明ffmpeg的安装路径

include_directories(${SDL_DIR}/include/) # 添加SDL头文件搜索路径
include_directories(${FFMPEG_DIR}/include/) # 添加ffmpeg头文件搜索路径

link_directories(${SDL_DIR}/lib/) # 增加SDL链接库目录
link_directories(${FFMPEG_DIR}/lib/) # 增加ffmpeg链接库目录

add_executable(sdl_test main.cpp)

target_link_libraries(sdl_test SDL2 SDL2_test SDL2main
        avcodec avdevice avfilter avformat avresample avutil postproc swresample swscale) # 链接目标库