#pragma once

#include <algorithm>
#include <numeric>
#include <stdint.h>
#include <stdexcept>
#include <vector>

#include "bitreader.h"
#include "context.h"

class MarkerHandler {
public:
    MarkerHandler(size_t limit) : limit_(limit) {
    }

    virtual void Handle(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) {
        IncreaseCounter();
        this->Process(reader, context);
    }

    virtual ~MarkerHandler() = default;

private:
    virtual void Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) = 0;

    virtual void IncreaseCounter() {
        if (limit_ > 0) {
            --limit_;
        } else {
            throw std::invalid_argument("Section appears too many times");
        }
    }

private:
    size_t limit_ = 0;
};

class SectionAPP final : public MarkerHandler {
public:
    constexpr static inline size_t kLimitOccurence = std::numeric_limits<size_t>::max();

    SectionAPP() : MarkerHandler(kLimitOccurence) {
    }

private:
    virtual void Process(BitReader<std::vector<uint8_t>>& /*reader*/, PictureContext* /*context*/
                         ) override {
        // chill
    }
};

class SectionDHT final : public MarkerHandler {
public:
    constexpr static inline size_t kLimitOccurence = std::numeric_limits<size_t>::max();

    SectionDHT() : MarkerHandler(kLimitOccurence) {
    }

private:
    virtual void Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) override;
};

class SectionDQT final : public MarkerHandler {
public:
    constexpr static inline size_t kLimitOccurence = std::numeric_limits<size_t>::max();

    SectionDQT() : MarkerHandler(kLimitOccurence) {
    }

private:
    virtual void Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) override;
};

class SectionCOM final : public MarkerHandler {
public:
    constexpr static inline size_t kLimitOccurence = std::numeric_limits<size_t>::max();

    SectionCOM() : MarkerHandler(kLimitOccurence) {
    }

private:
    virtual void Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) override;
};

class SectionSOF0 final : public MarkerHandler {
public:
    constexpr static inline size_t kLimitOccurence = 1;

    SectionSOF0() : MarkerHandler(kLimitOccurence) {
    }

private:
    virtual void Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) override;
};

class SectionSOS final : public MarkerHandler {
public:
    constexpr static inline size_t kLimitOccurence = 1;

    SectionSOS() : MarkerHandler(kLimitOccurence) {
    }

private:
    virtual void Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) override;
};
