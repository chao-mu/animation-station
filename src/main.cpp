// STL
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>


// OpenGL
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// TCLAP
#include <tclap/CmdLine.h>

// LibAV
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

// Ours
#include "Video.h"

void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, const char *filename);

/*
static void logging(const char *fmt, ...)
{
    va_list args;
    fprintf( stderr, "LOG: " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}
*/

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        exit(0);
    }

    const std::string path = argv[1];

    auto onFrame = [](AVFrame* /*pFrame*/) {
    };

    auto onErr = [](VideoError err) {
        std::cerr << "Error! " << err.value() << std::endl;
    };

    auto video = std::make_unique<Video>(path, onFrame, onErr);

    VideoError err = video->start();
    if (err.has_value()) {
        std::cerr << "Womp womp: " << err.value() << std::endl;
        return 1;
    }


    std::this_thread::sleep_for(std::chrono::seconds(2));

    video->stop();

    return 0;
}

// save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, frame_filename);
void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, const char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"w");
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // writing line by line
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}
