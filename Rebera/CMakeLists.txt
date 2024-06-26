# CMakeList.txt: Rebera 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
#ffmepg 路径
set(ffmpeg_path /home/gtj/reberaDepenOn/FFmpeg-n4.4.1/temp)
#x264路径
set(x264_path /home/gtj/reberaDepenOn/x264-master/temp)
set(lib_name  avformat avcodec avutil x264 pthread dl swresample SDL2)
#发送端代码路径
set(common_src_path  ${PROJECT_SOURCE_DIR}/Rebera/src/common)
set(send_src_path  ${PROJECT_SOURCE_DIR}/Rebera/src/sender)
set(recv_src_path  ${PROJECT_SOURCE_DIR}/Rebera/src/receiver)


include_directories(${PROJECT_SOURCE_DIR}/Rebera/src/include ${x264_path}/include ${ffmpeg_path}/include )
link_directories(${x264_path}/lib ${ffmpeg_path}/lib )

aux_source_directory(${send_src_path} send_src)
aux_source_directory(${recv_src_path} recv_src)
aux_source_directory(${common_src_path} common_src)
# 将源代码添加到此项目的可执行文件。
add_executable(send  ${send_src} ${common_src})
add_executable(recv  ${recv_src} ${common_src})
target_link_libraries(send ${lib_name} ${SDL2_LIBRARY})
target_link_libraries(recv ${lib_name} ${SDL2_LIBRARY})

