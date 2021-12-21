#pragma once

#include <iostream>
#include <optional>
#include <vector>

constexpr uint8_t kBitsInByte = 8;

template <class InputType>
class InputWrapper;

template <>
class InputWrapper<std::istream> {
public:
    InputWrapper(std::istream* input) : input_(input) {
    }

    bool ReadByte(uint8_t& byte) {
        input_->read(reinterpret_cast<char*>(&byte), 1);
        return input_->gcount();
    }

    bool IsEnd() const {
        return input_->eof();
    }

private:
    std::istream* input_;
};

template <>
class InputWrapper<std::vector<uint8_t>> {
public:
    InputWrapper(const std::vector<uint8_t>* input) : input_(input) {
    }

    bool ReadByte(uint8_t& byte) {
        if (index_ == input_->size()) {
            return false;
        } else {
            byte = (*input_)[index_++];
            return true;
        }
    }

    bool IsEnd() const {
        return index_ == input_->size();
    }

private:
    const std::vector<uint8_t>* input_;
    size_t index_ = 0;
};

template <class InputType>
class BitReader {
public:
    BitReader(InputType* input) : input_(InputWrapper<InputType>(input)) {
    }

    bool ReadBit() {
        if (position_ == kBitsInByte) {
            position_ = 0;
            if (!input_.ReadByte(buffer_)) {
                throw std::runtime_error("Cannot read, seems like EOF");
            }
        }
        return (1 << (kBitsInByte - 1 - position_++)) & buffer_;
    }

    uint8_t ReadHalfByte() {
        uint8_t result = (static_cast<uint8_t>(ReadBit()) << (kBitsInByte / 2 - 1)) +
                         (static_cast<uint8_t>(ReadBit()) << (kBitsInByte / 2 - 2)) +
                         (static_cast<uint8_t>(ReadBit()) << (kBitsInByte / 2 - 3)) +
                         static_cast<uint8_t>(ReadBit());
        return result;
    }

    uint8_t ReadByte() {
        uint8_t result = (buffer_ << position_);
        if (!input_.ReadByte(buffer_)) {
            throw std::runtime_error("Cannot read, seems like EOF");
        }
        result += (buffer_ >> (kBitsInByte - position_));
        return result;
    }

    uint16_t ReadDoubleByte() {
        uint16_t result = (ReadByte() << kBitsInByte);
        result += ReadByte();
        return result;
    }

    bool IsEnd() const {
        return input_.IsEnd();
    }

    template <class CharType>
    void FillVector(std::vector<CharType>& bytes) {
        for (size_t i = 0; i < bytes.size(); ++i) {
            bytes[i] = ReadByte();
        }
    }

    void FillString(std::string& str) {
        for (size_t i = 0; i < str.size(); ++i) {
            str[i] = ReadByte();
        }
    }

private:
    InputWrapper<InputType> input_;
    uint8_t buffer_ = 0;
    uint8_t position_ = 8;
};