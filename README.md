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
Introduction to Mitsuba

In our major project, we utilized Mitsuba 0.6 and implemented Bidirectional SPPM (BDSPPM) for global light rendering. The source files are located in /include/mitsuba and /src, and the implemented parts include:

src/librender/gatherproc.cpp

src/librender/particleproc.cpp

src/librender/photon.cpp

src/librender/photonmap.cpp

Corresponding .h files in /include/mitsuba/render

src/integrators/photonmapper/bdsppm.cpp

We also added a plugin in /src/integrators/Sconscript.

The executable file is mitsuba.exe in the /dist folder. You can render the scene corresponding to the .xml file using the command line ./dist/mitsuba -o xxxxx.xml, or use the GUI by running mtsgui.exe in the /dist folder, opening the .xml file, and starting the rendering process with real-time tracking.

Scene files and related .xml files are in /scenes. The .exr files with the same name as the .xml files are our existing rendering results.

###########################################################
Compilation method for Mitsuba 0.6:
If you encounter issues compiling and opening mtsgui.exe with the provided files, follow the steps below (based on our troubleshooting experience):

Compile Mitsuba 0.6 according to the original files (we did not keep the dependencies folder as it was too large). Refer to the tutorial at https://zhuanlan.zhihu.com/p/359008593 for specific instructions.
After compilation, if mtsgui.exe fails to run, delete the newly compiled /dist folder and unzip the dist archive in the root directory. The mtsgui.exe in this archive should run properly.
Replace the original include and src folders with the corresponding folders we provide.
Run scons for building, and you should be good to go.



