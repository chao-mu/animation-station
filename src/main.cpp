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

// SDL
extern "C" {
#include <SDL2/SDL.h>
}

// Ours
#include "Video.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        exit(0);
    }

    const std::string path = argv[1];

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) {
        std::cerr << "Could not initialize SDL - " <<  SDL_GetError() << std::endl;
        return 1;
    }

    auto video = std::make_shared<astation::Video>(path);
    astation::Error err = video->load();
    if (!err.empty()) {
        std::cerr << "Error loading video " << err << std::endl;
        return 1;
    }

    int screen_w = video->getWidth();
    int screen_h = video->getHeight();

    SDL_Window* screen = SDL_CreateWindow(
        "Animation Station",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        screen_w, screen_h,
        SDL_WINDOW_OPENGL
    );
    if (!screen) {
		printf("SDL: could not create window - exiting:%s\n",SDL_GetError());
        return 1;
    }

	SDL_Renderer* renderer = SDL_CreateRenderer(screen, -1, 0);
    SDL_Texture* texture = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);

    SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = screen_w;
	rect.h = screen_h;

    video->start();

    bool quit = false;
    while (!quit) {
        astation::Error err = video->loanFrame([texture, renderer, rect](AVFrame* pFrame) {
            SDL_UpdateYUVTexture(texture, &rect,
                    pFrame->data[0], pFrame->linesize[0],
                    pFrame->data[1], pFrame->linesize[1],
                    pFrame->data[2], pFrame->linesize[2]);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture,  NULL, &rect);
            SDL_RenderPresent(renderer);

            return "";
        });
        if (!err.empty()) {
            std::cerr << "Video error: " << err << std::endl;
            return 1;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            //If user closes the window
            if (e.type == SDL_QUIT){
                quit = true;
            }
        }

        SDL_Delay(30);
    }

    video->stop();

    SDL_Quit();

    return 0;
}
