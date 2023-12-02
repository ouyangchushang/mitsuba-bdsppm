Mitsuba — Physically Based Renderer
===================================

http://mitsuba-renderer.org/

## About

Mitsuba is a research-oriented rendering system in the style of PBRT, from which it derives much inspiration. It is written in portable C++, implements unbiased as well as biased techniques, and contains heavy optimizations targeted towards current CPU architectures. Mitsuba is extremely modular: it consists of a small set of core libraries and over 100 different plugins that implement functionality ranging from materials and light sources to complete rendering algorithms.

In comparison to other open source renderers, Mitsuba places a strong emphasis on experimental rendering techniques, such as path-based formulations of Metropolis Light Transport and volumetric modeling approaches. Thus, it may be of genuine interest to those who would like to experiment with such techniques that haven't yet found their way into mainstream renderers, and it also provides a solid foundation for research in this domain.

The renderer currently runs on Linux, MacOS X and Microsoft Windows and makes use of SSE2 optimizations on x86 and x86_64 platforms. So far, its main use has been as a testbed for algorithm development in computer graphics, but there are many other interesting applications.

Mitsuba comes with a command-line interface as well as a graphical frontend to interactively explore scenes. While navigating, a rough preview is shown that becomes increasingly accurate as soon as all movements are stopped. Once a viewpoint has been chosen, a wide range of rendering techniques can be used to generate images, and their parameters can be tuned from within the program.

## Documentation

For compilation, usage, and a full plugin reference, please see the [official documentation](http://mitsuba-renderer.org/docs.html).

## Releases and scenes

Pre-built binaries, as well as example scenes, are available on the [Mitsuba website](http://mitsuba-renderer.org/download.html).


##############################################
以上为mitsuba的介绍

我们在大作业中使用mitsuba0.6基于sppm实现了bdsppm全局光线渲染。
源文件在/include/mitsuba及/src中，我们实现/修改的部分有：

src/librender/gatherproc.cpp
src/librender/particleproc.cpp
src/librender/photon.cpp
src/librender/photonmap.cpp
以及include/mitsuba/render中对应的.h
src/integrators/photonmapper/bdsppm.cpp

并在src/integrators/Sconscript中添加了plugin

可执行文件为/dist文件夹中的mitsuba.exe，可使用命令行./dist/mitsuba -o xxxxx.xml来渲染.xml文件对应的场景；
或者使用gui，运行/dist文件夹中的mtsgui.exe并打开.xml文件，开始渲染后可实时跟踪进程。

场景文件及相关.xml文件在/scenes中，与.xml同名的.exr文件是我们已有的渲染结果。

###########################################################
编译mitsuba0.6的方法：
如果按照我们给的文件无法正常编译并打开mtsgui.exe，请采用以下方法（踩坑经验）：
首先按照原文件编译mitsuba0.6（dependencies文件夹过大，我们没有保留）
具体可参考教程https://zhuanlan.zhihu.com/p/359008593
在编译完成后，如果mtsgui.exe无法运行，请删除刚刚编译好的/dist文件夹，并解压缩在根目录下的dist压缩包，该压缩包中的mtsgui.exe可正常运行。
然后请将原来的include文件夹和src文件夹换成我们提供的同名文件夹
再运行scons进行构建，即可完成。

如有问题，欢迎联系组内成员！
祝您假期快乐！


