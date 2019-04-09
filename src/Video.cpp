#include "Video.h"

// LibAV
extern "C" {
#include <libavutil/error.h>
}

// https://github.com/joncampbell123/composite-video-simulator/issues/5
#undef av_err2str
#define av_err2str(errnum) std::string(av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum))

namespace astation {
    Video::Video(const std::string& path) : path_(path), running_(false) {}

    Video::~Video() {
        freeAll();
    }

    Error Video::start() {
        if (running_) {
            return "video is already running";
        }

        if (!loaded_) {
            Error err = load();
            if (err != "") {
                return err;
            }
        }

        running_ = true;

        our_thread_ = std::thread(&Video::loop, this);

        return {};
    }

    Error Video::load() {
        freeAll();

        format_ctx_ = avformat_alloc_context();
        if (!format_ctx_) {
            return "could not allocate memory for Format Context";
        }

        int resp = 0;

        resp = avformat_open_input(&format_ctx_, path_.c_str(), NULL, NULL);
        if (resp < 0) {
            return "could not open file: " + av_err2str(resp);
        }

        resp = avformat_find_stream_info(format_ctx_,  NULL);
        if (resp < 0) {
            return "could not get stream info: " + av_err2str(resp);
        }

        // loop though all the streams and print its main information
        video_stream_index_ = -1;
        AVCodecParameters* codec_Parameters;
        for (unsigned int i = 0; i < format_ctx_->nb_streams; i++)
        {
            codec_Parameters = format_ctx_->streams[i]->codecpar;
            codec_ = avcodec_find_decoder(codec_Parameters->codec_id);

            if (codec_ == nullptr) {
                return "unsupported codec!";
            }

            // when the stream is a video we store its index, codec parameters and codec
            if (codec_Parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index_ = i;
                break;
            }
        }

        if (video_stream_index_ < 0) {
            return "video stream not found";
        }

        codec_ctx_ = avcodec_alloc_context3(codec_);
        if (!codec_ctx_) {
            return "failed to allocate memory for AVCodecContext";
        }
        // Fill the codec context based on the values from the supplied codec parameters
        resp = avcodec_parameters_to_context(codec_ctx_, codec_Parameters);
        if (resp < 0) {
            return "failed to copy codec params to codec context: " + av_err2str(resp);
        }

        // Initialize the AVCodecContext to use the given AVCodec.
        resp = avcodec_open2(codec_ctx_, codec_, NULL);
        if (resp < 0) {
            return "failed to open codec through avcodec_open2: " + av_err2str(resp);
        }

        width_ = codec_ctx_->width;
        height_ = codec_ctx_->height;

        out_frame_ = av_frame_alloc();
        if (!out_frame_) {
            return "failed to allocate memory for output AVFrame";
        }

        frame_ = av_frame_alloc();
        if (!frame_) {
            return "failed to allocate memory for AVFrame";
        }

        packet_ = av_packet_alloc();
        if (!packet_) {
            return "failed to allocated memory for AVPacket";
        }

        loaded_ = true;

        return {};
    }

    void Video::stop() {
        if (running_) {
            running_ = false;
            our_thread_.join();
        }
    }

    void Video::freeAll() {
        if (format_ctx_ != nullptr) {
            avformat_close_input(&format_ctx_);
            avformat_free_context(format_ctx_);
            format_ctx_ = nullptr;
        }

        if (packet_ != nullptr) {
            av_packet_free(&packet_);
            packet_ = nullptr;
        }

        if (out_frame_ != nullptr) {
            av_frame_free(&out_frame_);
            frame_ = nullptr;
        }

        if (frame_ != nullptr) {
            av_frame_free(&frame_);
            frame_ = nullptr;
        }

        if (codec_ctx_ != nullptr) {
            avcodec_free_context(&codec_ctx_);
            codec_ctx_ = nullptr;
        }
    }

    void Video::loop() {
        last_processed_at_ = std::chrono::high_resolution_clock::now();
        last_pos_ = 0;

        while (running_) {
            int resp = av_read_frame(format_ctx_, packet_);
            if (resp == AVERROR_EOF) {
                AVStream* stream = format_ctx_->streams[video_stream_index_];
                avio_seek(format_ctx_->pb, 0, SEEK_SET);
                avformat_seek_file(format_ctx_, video_stream_index_, 0, 0, stream->duration, 0);
                // ???
                avcodec_flush_buffers(codec_ctx_);
                resp = av_read_frame(format_ctx_, packet_);
            }

            if (resp == AVERROR_EOF) {
                setError("end of file hit twice in a row, suggesting no data?");
                return;
            }

            if (resp < 0) {
                setError("failure to read frame: " + av_err2str(resp));
                return;
            }

            // if it's the video stream
            if (packet_->stream_index == video_stream_index_) {
                Error err = decodePacket();
                if (!err.empty()) {
                    av_packet_unref(packet_);
                    setError(err);
                    return;
                }
            }

            av_packet_unref(packet_);
        }

        // ???
        avcodec_flush_buffers(codec_ctx_);
    }

    Error Video::decodePacket() {
        // Supply raw packet data as input to a decoder
        // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga58bc4bf1e0ac59e27362597e467efff3
        int resp = avcodec_send_packet(codec_ctx_, packet_);
        if (resp < 0) {
            return "Error while sending a packet to the decoder: " + av_err2str(resp);
        }

        while (resp >= 0 && running_) {
            // Return decoded output data (into a frame) from a decoder
            // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga11e6542c4e66d3028668788a1a74217c

            resp = avcodec_receive_frame(codec_ctx_, frame_);
            if (resp == AVERROR(EAGAIN) || resp == AVERROR_EOF) {
                break;
            } else if (resp < 0) {
                return "Error while receiving a frame from the decoder: " + av_err2str(resp);
            }

            if (resp >= 0) {
                AVStream* stream = format_ctx_->streams[video_stream_index_];
                int64_t pos = av_rescale_q(frame_->pts, stream->time_base, AV_TIME_BASE_Q);
                int64_t delay = pos - last_pos_;

                int64_t elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now() - last_processed_at_).count();
                if (elapsed < delay) {
                    std::this_thread::sleep_for(std::chrono::microseconds(delay - elapsed));
                }

                last_processed_at_ = std::chrono::high_resolution_clock::now();
                last_pos_ = pos;

                {
                    std::lock_guard<std::mutex> guard(out_mutex_);
                    std::swap(frame_, out_frame_);
                }

                av_frame_unref(frame_);
            }
        }

        return {};
    }

    Error Video::loanFrame(FrameCallback cb) {
        std::lock_guard<std::mutex> guard(out_mutex_);
        if (!out_err_.empty()) {
            Error err = out_err_;
            out_err_ = "";
            return err;
        }

        return cb(out_frame_);
    }

    void Video::setError(Error err) {
        std::lock_guard<std::mutex> guard(out_mutex_);
        out_err_ = err;
    }

    int Video::getWidth() {
        return width_;
    }

    int Video::getHeight() {
        return height_;
    }
}
