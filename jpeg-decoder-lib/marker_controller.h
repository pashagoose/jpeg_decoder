#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <sstream>
#include <unordered_map>

#include "bitreader.h"
#include "marker_handlers.h"
#include "context.h"

enum class SectionID {
    SOI = 0xFFD8,   // start of image
    EOI = 0xFFD9,   // end of image
    COM = 0xFFFE,   // commentary
    DQT = 0xFFDB,   // define quantization table
    DHT = 0xFFC4,   // define huffman table
    SOF0 = 0xFFC0,  // meta information about image
    SOS = 0xFFDA,   // start of scan
    APP = 0xFFE0,   // app information (ignored in this implementation)
    INVALID = 0xFF00,
};

class MarkerOrderComparator {
public:
    MarkerOrderComparator(std::initializer_list<std::pair<SectionID, size_t>> init_list)
        : section_ids_(init_list.begin(), init_list.end()) {
    }

    bool operator()(SectionID lhs, SectionID rhs) {
        return (section_ids_[lhs] < section_ids_[rhs]);
    }

private:
    std::unordered_map<SectionID, size_t> section_ids_;
};

class MarkerFactory {
public:
    MarkerFactory();

    void Handle(SectionID marker, BitReader<std::vector<uint8_t>>& reader, PictureContext* context);

private:
    std::unordered_map<SectionID, std::unique_ptr<MarkerHandler>> handlers_;
};

template <typename T>
std::string NumToHexString(T val) {
    std::stringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

class MarkerController {  // separates binary data for markers
                          // (thanks for unspecified order of sections in jpeg!)
public:
    MarkerController(std::istream* input, PictureContext* context)
        : reader_(input), context_(context) {
    }

    void SeparateAndProcess();

private:
    BitReader<std::istream> reader_;
    std::vector<std::vector<uint8_t>> sections_;
    PictureContext* context_;
};