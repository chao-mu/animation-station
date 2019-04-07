#include "Video.h"

// LibAV
extern "C" {
#include <libavutil/error.h>
}

// https://github.com/joncampbell123/composite-video-simulator/issues/5
#undef av_err2str
#define av_err2str(errnum) std::string(av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum))

Video::~Video() {
    freeAll();
}

Video::Video(const std::string& path) : path_(path), running_(false) {}

VideoError Video::start() {
    if (running_) {
        return "video is already running";
    }

    if (!loaded_) {
        VideoError err = load();
        if (err != "") {
            return err;
        }
    }

    running_ = true;

    our_thread_ = std::thread(&Video::loop, this);

    return {};
}

VideoError Video::load() {
    freeAll();

    pFormatContext = avformat_alloc_context();
    if (!pFormatContext) {
        return "could not allocate memory for Format Context";
    }

    int resp = 0;

    resp = avformat_open_input(&pFormatContext, path_.c_str(), NULL, NULL);
    if (resp < 0) {
        return "could not open file: " + av_err2str(resp);
    }

    resp = avformat_find_stream_info(pFormatContext,  NULL);
    if (resp < 0) {
        return "could not get stream info: " + av_err2str(resp);
    }

    // loop though all the streams and print its main information
    video_stream_index_ = -1;
    AVCodecParameters* pCodecParameters;
    for (unsigned int i = 0; i < pFormatContext->nb_streams; i++)
    {
        pCodecParameters = pFormatContext->streams[i]->codecpar;
        pCodec = avcodec_find_decoder(pCodecParameters->codec_id);

        if (pCodec == nullptr) {
            return "unsupported codec!";
        }

        // when the stream is a video we store its index, codec parameters and codec
        if (pCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

    if (video_stream_index_ < 0) {
        return "video stream not found";
    }

    pCodecContext = avcodec_alloc_context3(pCodec);
    if (!pCodecContext) {
        return "failed to allocate memory for AVCodecContext";
    }
    // Fill the codec context based on the values from the supplied codec parameters
    resp = avcodec_parameters_to_context(pCodecContext, pCodecParameters);
    if (resp < 0) {
        return "failed to copy codec params to codec context: " + av_err2str(resp);
    }

    // Initialize the AVCodecContext to use the given AVCodec.
    resp = avcodec_open2(pCodecContext, pCodec, NULL);
    if (resp < 0) {
        return "failed to open codec through avcodec_open2: " + av_err2str(resp);
    }

	width_ = pCodecContext->width;
	height_ = pCodecContext->height;

    out_frame_ = av_frame_alloc();
    if (!out_frame_) {
        return "failed to allocate memory for output AVFrame";
    }

    pFrame = av_frame_alloc();
    if (!pFrame) {
        return "failed to allocate memory for AVFrame";
    }

    pPacket = av_packet_alloc();
    if (!pPacket) {
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
    if (pFormatContext != nullptr) {
        avformat_close_input(&pFormatContext);
        avformat_free_context(pFormatContext);
        pFormatContext = nullptr;
    }

    if (pPacket != nullptr) {
        av_packet_free(&pPacket);
        pPacket = nullptr;
    }

    if (out_frame_ != nullptr) {
        av_frame_free(&out_frame_);
        pFrame = nullptr;
    }

    if (pFrame != nullptr) {
        av_frame_free(&pFrame);
        pFrame = nullptr;
    }

    if (pCodecContext != nullptr) {
        avcodec_free_context(&pCodecContext);
        pCodecContext = nullptr;
    }
}

void Video::loop() {
    last_processed_at_ = std::chrono::high_resolution_clock::now();
    last_pos_ = 0;

    while (running_) {
        int resp = av_read_frame(pFormatContext, pPacket);
        if (resp == AVERROR_EOF) {
            AVStream* stream = pFormatContext->streams[video_stream_index_];
            avio_seek(pFormatContext->pb, 0, SEEK_SET);
            avformat_seek_file(pFormatContext, video_stream_index_, 0, 0, stream->duration, 0);
            // ???
            avcodec_flush_buffers(pCodecContext);
            resp = av_read_frame(pFormatContext, pPacket);
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
        if (pPacket->stream_index == video_stream_index_) {
            VideoError err = decodePacket();
            if (!err.empty()) {
                av_packet_unref(pPacket);
                setError(err);
                return;
            }
        }

        av_packet_unref(pPacket);
    }

    // ???
    avcodec_flush_buffers(pCodecContext);
}

VideoError Video::decodePacket() {
    // Supply raw packet data as input to a decoder
    // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga58bc4bf1e0ac59e27362597e467efff3
    int resp = avcodec_send_packet(pCodecContext, pPacket);
    if (resp < 0) {
        return "Error while sending a packet to the decoder: " + av_err2str(resp);
    }

    while (resp >= 0 && running_) {
        // Return decoded output data (into a frame) from a decoder
        // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga11e6542c4e66d3028668788a1a74217c

        resp = avcodec_receive_frame(pCodecContext, pFrame);
        if (resp == AVERROR(EAGAIN) || resp == AVERROR_EOF) {
            break;
        } else if (resp < 0) {
            return "Error while receiving a frame from the decoder: " + av_err2str(resp);
        }

        if (resp >= 0) {
            AVStream* stream = pFormatContext->streams[video_stream_index_];
            int64_t pos = av_rescale_q(pFrame->pts, stream->time_base, AV_TIME_BASE_Q);
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
                std::swap(pFrame, out_frame_);
            }

            av_frame_unref(pFrame);
        }
    }

    return {};
}

VideoError Video::loanFrame(FrameCallback cb) {
    std::lock_guard<std::mutex> guard(out_mutex_);
    if (!out_err_.empty()) {
        VideoError err = out_err_;
        out_err_ = "";
        return err;
    }

    return cb(out_frame_);
}

void Video::setError(VideoError err) {
    std::lock_guard<std::mutex> guard(out_mutex_);
    out_err_ = err;
}

int Video::getWidth() {
    return width_;
}

int Video::getHeight() {
    return height_;
}
