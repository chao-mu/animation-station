#ifndef ASTATION_TYPES_H_
#define ASTATION_TYPES_H_

namespace astation {
    using Error = std::string;
    using FrameCallback = std::function<Error(AVFrame*)>;
}

#endif // ASTATION_TYPES_H_
