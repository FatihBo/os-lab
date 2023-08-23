南京大学课程《操作系统设计与实现》的课程实验代码

课程主页：

https://jyywiki.cn/OS/2022/index.html



该课程分为minlab和lab，其中minlab是在已有的操作系统上实现部分功能

lab是在裸机(bare-mental)上实现，需要在QEMU环境下模拟



abstract-machine: 课程提供的框架，提供了OS中最底层的内容，包括boot，以及基础的api

amgame: 在裸机上实现的小游戏

picture: 在裸机上现实图片(可用于开机界面)

kernel: 模拟操作系统内存管理

libco: 实现协程(模拟线程的本质)

pstree: 模拟Linux中的pstree

static_loader.c : 静态加载器功能模拟

sperf: 模拟profiler



