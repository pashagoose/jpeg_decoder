#include "context.h"

#include <glog/logging.h>
#include <stdexcept>
#include "bitreader.h"

void DiagonalUnitIterator::Proceed() {
    if ((x_ + y_) % 2) {
        if (x_ + 1 == kDataUnitSide) {
            ++y_;
        } else if (y_ == 0) {
            ++x_;
        } else {
            ++x_;
            --y_;
        }
    } else {
        if (y_ + 1 == kDataUnitSide) {
            ++x_;
        } else if (x_ == 0) {
            ++y_;
        } else {
            --x_;
            ++y_;
        }
    }
}

bool DiagonalUnitIterator::IsEnd() {
    return (x_ >= kDataUnitSide || y_ >= kDataUnitSide);
}

DataUnit::Iterator& DataUnit::Iterator::operator++() {
    gut_it_.Proceed();
    return *this;
}

double& DataUnit::Iterator::operator*() {
    auto [x, y] = gut_it_.Get();
    return (*block_)[kDataUnitSide * x + y];
}

bool DataUnit::Iterator::IsEnd() {
    return gut_it_.IsEnd();
}

void DataUnit::Read(BitReader<std::vector<uint8_t>>& reader, HuffmanTree dc_tree,
                    HuffmanTree ac_tree) {

    Iterator it(&block_);

    auto read_coef = [&](size_t len) {
        if (len == 0) {
            return static_cast<int16_t>(0);
        }

        int16_t result = 1;

        bool negative = reader.ReadBit() ^ 1;
        --len;

        while (len--) {
            result *= 2;
            result += negative ^ reader.ReadBit();
        }

        if (negative) {
            result = -result;
        }

        return result;
    };

    // read 1 DC coefficient and 63 AC coefficients

    int val;
    while (!dc_tree.Move(reader.ReadBit(), val)) {
    }
    (*it) = read_coef(static_cast<size_t>(val));
    ++it;

    while (!it.IsEnd()) {
        while (!ac_tree.Move(reader.ReadBit(), val)) {
        }

        size_t nulls = static_cast<uint8_t>(val) >> (kBitsInByte / 2);
        size_t len = (static_cast<uint8_t>(val) & 0xf);

        if (nulls == 0 && len == 0) {
            // only zeros left
            while (!it.IsEnd()) {
                (*it) = 0;
                ++it;
            }
            break;
        }

        int16_t coef = read_coef(len);

        while (nulls--) {
            if (it.IsEnd()) {
                throw std::invalid_argument("Too much zeros in data unit");
            }
            (*it) = 0;
            ++it;
        }

        if (it.IsEnd()) {
            throw std::invalid_argument("Not enough space for coefficient in data unit");
        }
        (*it) = coef;
        ++it;
    }

    CHECK(it.IsEnd());
}

MCUBlock::MCUBlock(size_t height, size_t width, PictureContext* context,
                   std::vector<HuffmanTree>&& dc_trees, std::vector<HuffmanTree>&& ac_trees)
    : height_(height),
      width_(width),
      previous_dcs_(context->channels.size(), 0),
      unit_before_idct_(),
      unit_after_idct_(),
      context_(context),
      dc_trees_(dc_trees),
      ac_trees_(ac_trees),
      idct_executor_(kDataUnitSide, &unit_before_idct_.block_, &unit_after_idct_.block_),
      picture_piece_(height, width) {
}

size_t MCUBlock::GetHeight() const {
    return height_;
}

size_t MCUBlock::GetWidth() const {
    return width_;
}

void MCUBlock::Dequantize(size_t qt_id) {
    auto it = context_->qts.find(qt_id);
    if (it == context_->qts.end()) {
        throw std::invalid_argument("No QT with id: " + std::to_string(qt_id));
    }
    auto& qt = it->second;

    for (size_t j = 0; j < kDataUnitSide * kDataUnitSide; ++j) {
        unit_before_idct_.block_[j] *= qt[j];
    }
}

void MCUBlock::ConvertToUnsignedScale(size_t precision) {
    uint32_t shift = (1 << (precision - 1));

    for (size_t j = 0; j < kDataUnitSide * kDataUnitSide; ++j) {
        unit_after_idct_.block_[j] = round(unit_after_idct_.block_[j]);
        unit_after_idct_.block_[j] += shift;
        unit_after_idct_.block_[j] = std::min(unit_after_idct_.block_[j], (1 << precision) - 1.0);
        unit_after_idct_.block_[j] = std::max(unit_after_idct_.block_[j], 0.0);
    }
}

void MCUBlock::ConvertToRGB(size_t x, size_t y, size_t channel_id) {
    // x, y - offsets in MCU
    // YCbCr -> RGB
    size_t prolong_w = context_->channels[channel_id].horizontal_thinning;
    size_t prolong_h = context_->channels[channel_id].vertical_thinning;

    for (size_t xshift = 0; xshift < kDataUnitSide; ++xshift) {
        for (size_t yshift = 0; yshift < kDataUnitSide; ++yshift) {
            size_t i = x + xshift * prolong_h, j = y + yshift * prolong_w;

            for (size_t h_add = 0; h_add < prolong_h; ++h_add) {
                for (size_t w_add = 0; w_add < prolong_w; ++w_add) {
                    double val = unit_after_idct_.Get(xshift, yshift);
                    if (channel_id == static_cast<size_t>(ChannelNames::Y)) {
                        picture_piece_.r[i + h_add][j + w_add] += val;
                        picture_piece_.g[i + h_add][j + w_add] += val;
                        picture_piece_.b[i + h_add][j + w_add] += val;
                    } else if (channel_id == static_cast<size_t>(ChannelNames::Cb)) {
                        picture_piece_.g[i + h_add][j + w_add] += -0.34414 * (val - 128);
                        picture_piece_.b[i + h_add][j + w_add] += 1.772 * (val - 128);
                    } else if (channel_id == static_cast<size_t>(ChannelNames::Cr)) {
                        picture_piece_.r[i + h_add][j + w_add] += 1.402 * (val - 128);
                        picture_piece_.g[i + h_add][j + w_add] += -0.71414 * (val - 128);
                    } else {
                        throw std::invalid_argument("Unrecognized channel id `" +
                                                    std::to_string(channel_id) +
                                                    "` while flushing to image:");
                    }
                }
            }
        }
    }
}

void RGBBlock::Clear() {
    size_t height = r.size();
    CHECK(height != 0) << "WTF, MCU height is zero?!";

    size_t width = r[0].size();
    r.assign(height, std::vector<double>(width, 0));
    g.assign(height, std::vector<double>(width, 0));
    b.assign(height, std::vector<double>(width, 0));
}

void RGBBlock::FlushToImage(size_t y, size_t x, size_t precision, Image& img) {
    size_t height = img.Height();
    size_t width = img.Width();

    for (size_t i = 0; i < r.size(); ++i) {
        for (size_t j = 0; j < r[i].size(); ++j) {
            if (y + i >= height || x + j >= width) {
                break;
            }

            auto& pix = img.GetPixel(y + i, x + j);
            pix.r = std::max(0.0, std::min((1 << precision) - 1.0, round(r[i][j])));
            pix.g = std::max(0.0, std::min((1 << precision) - 1.0, round(g[i][j])));
            pix.b = std::max(0.0, std::min((1 << precision) - 1.0, round(b[i][j])));
        }
    }
}

void MCUBlock::Process(BitReader<std::vector<uint8_t>>& reader, size_t x, size_t y) {
    picture_piece_.Clear();

    for (size_t i = 0; i < context_->channels.size(); ++i) {
        double& prev_dc = previous_dcs_[i];
        size_t height_multiplier =
            height_ / (context_->channels[i].vertical_thinning * kDataUnitSide);
        size_t width_multiplier =
            width_ / (context_->channels[i].horizontal_thinning * kDataUnitSide);

        for (size_t j = 0; j < height_multiplier; ++j) {
            for (size_t k = 0; k < width_multiplier; ++k) {
                unit_before_idct_.Read(reader, dc_trees_[i], ac_trees_[i]);

                unit_before_idct_.block_[0] +=
                    prev_dc;  // read DC coef is shift relative to previous DC coef
                prev_dc = unit_before_idct_.block_[0];

                Dequantize(context_->channels[i].qt_id);

                idct_executor_.Inverse();

                ConvertToUnsignedScale(context_->precision);

                ConvertToRGB(j * kDataUnitSide, k * kDataUnitSide, i);
            }
        }
    }

    picture_piece_.FlushToImage(x, y, context_->precision, context_->image);
}

MCUIterator::MCUIterator(std::vector<HuffmanTree>&& dc_trees, std::vector<HuffmanTree>&& ac_trees,
                         PictureContext* context)
    : block_(context->mcu_height, context->mcu_width, context, std::move(dc_trees),
             std::move(ac_trees)),
      context_(context) {
}

MCUIterator& MCUIterator::operator++() {
    y_ += context_->mcu_width;
    if (y_ >= context_->width) {
        y_ = 0;
        x_ += context_->mcu_height;
    }
    return *this;
}

MCUBlock* MCUIterator::operator->() {
    return &block_;
}

void MCUIterator::Process(BitReader<std::vector<uint8_t>>& reader) {
    block_.Process(reader, x_, y_);
}

bool MCUIterator::IsEnd() {
    return (y_ >= context_->width || x_ >= context_->height);
}

MCUIterator PictureContext::GetMCUBeginIterator(std::vector<HuffmanTree>&& dc_trees,
                                                std::vector<HuffmanTree>&& ac_trees) {
    return MCUIterator(std::move(dc_trees), std::move(ac_trees), this);
}