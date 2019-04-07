// STL
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

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
    //av_register_all();

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        exit(0);
    }

    const std::string path = argv[1];

    auto onFrame = [](AVFrame* /*pFrame*/) {
    };

    auto onErr = [](VideoError err) {
        std::cerr << "Error! " << err << std::endl;
    };

    std::vector<std::shared_ptr<Video>> videos;
    for (int i = 0; i < 100; i++) {
        auto video = std::make_shared<Video>(path, onFrame, onErr);
        videos.push_back(video);

        VideoError err = video->load();
        if (!err.empty()) {
            std::cerr << "Womp womp: " << err << std::endl;
            return 1;
        }
    }

    for (int i = 0; i < 4; i++) {
        videos[i]->start();
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    for (int i = 0; i < 4; i++) {
        videos[i]->stop();
    }

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
