#ifndef ASTATION_VIDEO_H_
#define ASTATION_VIDEO_H_

// STL
#include <atomic>
#include <functional>
#include <thread>
#include <chrono>
#include <mutex>

// LibAV
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

// Ours
#include "types.h"

namespace astation {
    class Video {
        public:
            Video(const std::string& path);
            ~Video();

            // start playback
            Error start();

            // pause playback
            void stop();

            // Load resources
            Error load();

            Error loanFrame(FrameCallback cb);

            int getWidth();
            int getHeight();

        private:
            // Report error
            void setError(Error err);

            // Free allocated resources
            void freeAll();

            // The main processing loop to be launched in a thread
            void loop();

            // Path to video
            const std::string path_;

            // Prepare our frame
            Error decodePacket();

            // The index of the video stream in format's context's streams
            int video_stream_index_ = -1;

            // The component that knows how to encode/decode our stream
            AVCodec* codec_ = nullptr;

            // A frame from our stream (use out_mutex_ on access)
            AVFrame* out_frame_ = nullptr;

            // the video container (also known as a "format")
            AVFormatContext* format_ctx_ = nullptr;

            // This structure stores compressed data
            AVPacket* packet_ = nullptr;

            // hold the context for our decode/encode process
            AVCodecContext* codec_ctx_ = nullptr;

            // Whether or not we're running
            std::atomic<bool> running_;

            // Last encountered error
            Error out_err_ = "";

            // The frame we are outputting
            AVFrame* frame_ = nullptr;

            // Dimensions
            int width_;
            int height_;

            bool loaded_ = false;

            std::thread our_thread_;

            int64_t last_pos_ = 0;
            std::chrono::time_point<std::chrono::high_resolution_clock> last_processed_at_;

            // Locking out_* variables (shared varaibles)
            std::mutex out_mutex_;
    };
}
#endif // ASTATION_VIDEO_H_
