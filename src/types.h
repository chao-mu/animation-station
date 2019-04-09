#ifndef ASTATION_TYPES_H
#define ASTATION_TYPES_H

namespace astation {
    using Error = std::string;
    using FrameCallback = std::function<Error(AVFrame*)>;
}

#endif

