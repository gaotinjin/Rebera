---
auther: GaoTinjin
title: rebera 编译
sidebar_label: 1.0
---
# 一、编译x264
x264用于采集usb相机画面，视频编码为264码流。</br>
1、创建目录
```
mkdir ~/dependOn
```
2、git拉取源码
```
git clone https://github.com/mirror/x264.git
```
3、指定生成目录为本地文件的temp目录
```
./configure --prefix=./temp --enable-shared
```
4、开始编译
```
make -j8 && make install
```
*注意：如果编译报错nasm缺失，直接执行下面代码*
```
sudo apt-get install nasm
```
最后，在生成的temp目录下应该有以下文件：
```
gtj@ubuntu:~/reberaDepenOn/x264-master/temp$ tree
.
├── bin
│   └── x264
├── include
│   ├── x264_config.h
│   └── x264.h
└── lib
    ├── libx264.so -> libx264.so.164
    ├── libx264.so.164
    └── pkgconfig
        └── x264.pc
```
# 二、编译ffmpeg
1、安装SDL2，用于显示。
```
sudo apt-get install libsdl2-2.0
sudo apt-get install libsdl2-dev
```
2、下载ffmpeg源代码，这里我下的是n4.4.1版本
```
git clone https://github.com/FFmpeg/FFmpeg.git
```
接下来执行
```
./configure --prefix=./temp --enable-shared --enable-libx264 --enable-gpl --enable-pthreads --enable-sdl2 --extra-cflags=-I/（x264文件夹所在位置自己的不要复制)/temp/include --extra-ldflags=-L/（x264文件夹所在位置自己的不要复制)/temp/lib
```
*目的是改动ffmepg配置添加X264支持和SDL支持*
3、开始编译
```
make -j8 && make install
```
最后生成temp文件夹。
