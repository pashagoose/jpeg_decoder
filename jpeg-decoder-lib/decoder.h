#pragma once

#include "utils/image.h"
#include "context.h"
#include "marker_controller.h"

#include <istream>

Image Decode(std::istream& input);

class Decoder {
public:
    Decoder(std::istream& input) : controller_(&input, &context_) {
    }

    Image Decode();

private:
    PictureContext context_;
    MarkerController controller_;
};
