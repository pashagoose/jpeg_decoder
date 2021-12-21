#include "decoder.h"

Image Decode(std::istream& input) {
    Decoder decoder(input);

    return decoder.Decode();
}

Image Decoder::Decode() {
    controller_.SeparateAndProcess();
    return context_.image;
}
