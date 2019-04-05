// STL
#include <iostream>

// OpenGL
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// TCLAP
#include <tclap/CmdLine.h>

// OpenCV
#include <opencv2/opencv.hpp>

// LibAV
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int main(int argc, char **argv)
{
    av_register_all();

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        exit(0);
    }
    //const char* path = argv[1];


    return 0;
}
