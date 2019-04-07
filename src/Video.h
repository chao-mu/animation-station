#ifndef ASTATION_VIDEO_H
#define ASTATION_VIDEO_H

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

using VideoError = std::string;
using FrameCallback = std::function<VideoError(AVFrame*)>;

class Video {
    public:
        Video(const std::string& path);
        ~Video();

        // start playback
        VideoError start();

        // pause playback
        void stop();

        // Load resources
        VideoError load();

        VideoError loanFrame(FrameCallback cb);

        int getWidth();
        int getHeight();

    private:
        // Report error
        void setError(VideoError err);

        // Free allocated resources
        void freeAll();

        // The main processing loop to be launched in a thread
        void loop();

        // Path to video
        const std::string path_;

        // Prepare our frame
        VideoError decodePacket();

        // The index of the video stream in format's context's streams
        int video_stream_index_ = -1;

        // The component that knows how to encode/decode our stream
        AVCodec* pCodec = nullptr;

        // A frame from our stream (use out_mutex_ on access)
        AVFrame* out_frame_ = nullptr;

        // the video container (also known as a "format")
        AVFormatContext* pFormatContext = nullptr;

        // This structure stores compressed data
        AVPacket* pPacket = nullptr;

        // hold the context for our decode/encode process
        AVCodecContext* pCodecContext = nullptr;

        // Whether or not we're running
        std::atomic<bool> running_;

        // Last encountered error
        VideoError out_err_ = "";

        // The frame we are outputting
        AVFrame* pFrame = nullptr;

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

#endif
