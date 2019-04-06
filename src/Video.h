#ifndef ASTATION_VIDEO_H
#define ASTATION_VIDEO_H

// STL
#include <atomic>
#include <functional>
#include <optional>
#include <thread>
#include <chrono>

// LibAV
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

using VideoError = std::optional<std::string>;
using FrameCallback = std::function<void(AVFrame*)>;

class Video {
    public:
        Video(const std::string& path, FrameCallback onFrame, std::function<void(VideoError)> onErr);
        ~Video();
        VideoError start();
        void stop();

    private:
        // Free allocated resources
        void freeAll();

        // The main processing loop to be launched in a thread
        void loop();

        // Load resources
        VideoError load();

        // Path to video
        const std::string path_;

        // Callbacks
        FrameCallback on_frame_;
        std::function<void(VideoError)> on_err_;

        // Prepare our frame
        VideoError decodePacket();

        // The index of the video stream in format's context's streams
        int video_stream_index_ = -1;

        // The component that knows how to encode/decode our stream
        AVCodec* pCodec = nullptr;

        // A frame from our stream
        AVFrame* pFrame = nullptr;

        // the video container (also known as a "format")
        AVFormatContext* pFormatContext = nullptr;

        // This structure stores compressed data
        AVPacket* pPacket = nullptr;

        // hold the context for our decode/encode process
        AVCodecContext* pCodecContext = nullptr;

        // Whether or not we're running
        std::atomic<bool> running_ = false;

        bool loaded_ = false;

        std::thread our_thread_;

        int64_t last_pos_ = 0;
        std::chrono::time_point<std::chrono::high_resolution_clock> last_processed_at_;
};

#endif
