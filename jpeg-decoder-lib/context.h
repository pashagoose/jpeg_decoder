#pragma once

#include <unordered_map>

#include "huffman.h"
#include "bitreader.h"
#include "fft.h"
#include "utils/image.h"

constexpr uint8_t kDataUnitSide = 8;

enum class ChannelNames {
    Y = 0,
    Cb = 1,
    Cr = 2,
};

struct Channel {
    uint8_t horizontal_thinning;
    uint8_t vertical_thinning;
    uint8_t qt_id;  // quantization table id
};

struct RGBBlock {
    RGBBlock(size_t height, size_t width)
        : r(height, std::vector<double>(width, 0)),
          g(height, std::vector<double>(width, 0)),
          b(height, std::vector<double>(width, 0)) {
    }

    void Clear();
    void FlushToImage(size_t y, size_t x, size_t precision, Image& img);

    std::vector<std::vector<double>> r, g, b;
};

class DiagonalUnitIterator {
public:
    void Proceed();
    bool IsEnd();

    std::pair<size_t, size_t> Get() const {
        return {x_, y_};
    }

private:
    size_t x_ = 0;
    size_t y_ = 0;
};

class DataUnit {
    /*
            8x8 block for some channel
    */
public:
    friend class MCUBlock;

    DataUnit() : block_(kDataUnitSide * kDataUnitSide) {
    }

    void Read(BitReader<std::vector<uint8_t>>& reader, HuffmanTree dc_tree, HuffmanTree ac_tree);

    class Iterator {  // for traversing diagonally
    public:
        Iterator(std::vector<double>* block) : block_(block) {
        }

        Iterator& operator++();
        double& operator*();

        bool IsEnd();

    private:
        DiagonalUnitIterator gut_it_;
        std::vector<double>* block_;
    };

    double& Get(size_t i, size_t j) {
        return block_[i * kDataUnitSide + j];
    }

    const double& Get(size_t i, size_t j) const {
        return block_[i * kDataUnitSide + j];
    }

private:
    std::vector<double> block_;  // this is square matrix
};

class PictureContext;

class MCUBlock {
    /*
            Minimum coded unit consists of DataUnit objects for each channel.
            MCU can contain several DataUnit objects for some channel.
    */
public:
    MCUBlock(size_t height, size_t width, PictureContext* context,
             std::vector<HuffmanTree>&& dc_trees, std::vector<HuffmanTree>&& ac_trees);

    void Process(BitReader<std::vector<uint8_t>>& reader, size_t x, size_t y);

    size_t GetHeight() const;
    size_t GetWidth() const;

private:
    void Dequantize(size_t qt_id);  // inverse quantization
    void ConvertToUnsignedScale(size_t precision);
    void ConvertToRGB(size_t x, size_t y, size_t channel_id);

private:
    size_t height_;
    size_t width_;
    std::vector<double> previous_dcs_;
    DataUnit unit_before_idct_;  // we do not store all units, we only need one unit at each moment
    DataUnit unit_after_idct_;
    PictureContext* context_;
    std::vector<HuffmanTree> dc_trees_;
    std::vector<HuffmanTree> ac_trees_;
    DctCalculator idct_executor_;
    RGBBlock picture_piece_;
};

class MCUIterator {
    /*
            Forward iterator, each iterator owns the MCU.
            We do not store all MCUs, every time we have finished processing block -
                                    - flush it to the Image object.
    */
public:
    MCUIterator(std::vector<HuffmanTree>&& dc_trees, std::vector<HuffmanTree>&& ac_trees,
                PictureContext* context);

    MCUIterator& operator++();
    MCUBlock* operator->();

    void Process(BitReader<std::vector<uint8_t>>& reader);

    bool IsEnd();

private:
    size_t x_ = 0;  // coordinates of top-left point of MCU
    size_t y_ = 0;
    MCUBlock block_;
    PictureContext* context_;
};

class PictureContext {
public:
    MCUIterator GetMCUBeginIterator(std::vector<HuffmanTree>&& dc_trees,
                                    std::vector<HuffmanTree>&& ac_trees);

public:
    Image image;
    uint8_t precision;
    uint16_t height;
    uint16_t width;
    uint8_t mcu_height;
    uint8_t mcu_width;
    std::vector<Channel> channels;
    std::unordered_map<uint8_t, HuffmanTree> ac_huffman_trees;
    std::unordered_map<uint8_t, HuffmanTree> dc_huffman_trees;
    std::unordered_map<uint8_t, std::vector<uint16_t>> qts;  // quantization tables
};