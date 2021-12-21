#include "marker_controller.h"

#include <memory>
#include <stdexcept>
#include <glog/logging.h>

bool IsAppMarker(uint16_t marker_num) {
    return (marker_num >= 0xFFE0 && marker_num <= 0xFFEF);
}

SectionID DoubleByteToMarker(uint16_t num) {
    if (num == static_cast<uint16_t>(SectionID::COM)) {
        return SectionID::COM;
    } else if (num == static_cast<uint16_t>(SectionID::DHT)) {
        return SectionID::DHT;
    } else if (num == static_cast<uint16_t>(SectionID::DQT)) {
        return SectionID::DQT;
    } else if (num == static_cast<uint16_t>(SectionID::SOS)) {
        return SectionID::SOS;
    } else if (num == static_cast<uint16_t>(SectionID::SOF0)) {
        return SectionID::SOF0;
    } else if (num == static_cast<uint16_t>(SectionID::SOI)) {
        return SectionID::SOI;
    } else if (num == static_cast<uint16_t>(SectionID::EOI)) {
        return SectionID::EOI;
    } else if (IsAppMarker(num)) {
        return SectionID::APP;
    } else {
        return SectionID::INVALID;
    }
}

MarkerFactory::MarkerFactory() {
    handlers_[SectionID::SOS] = std::make_unique<SectionSOS>();
    handlers_[SectionID::COM] = std::make_unique<SectionCOM>();
    handlers_[SectionID::SOF0] = std::make_unique<SectionSOF0>();
    handlers_[SectionID::DQT] = std::make_unique<SectionDQT>();
    handlers_[SectionID::DHT] = std::make_unique<SectionDHT>();
    handlers_[SectionID::APP] = std::make_unique<SectionAPP>();
}

void MarkerFactory::Handle(SectionID marker, BitReader<std::vector<uint8_t>>& reader,
                           PictureContext* context) {
    auto it = handlers_.find(marker);
    CHECK(it != handlers_.end());

    it->second->Handle(reader, context);
}

void MarkerController::SeparateAndProcess() {
    DLOG(INFO) << "Start separating markers content to buffers";

    if (DoubleByteToMarker(reader_.ReadDoubleByte()) != SectionID::SOI) {
        throw std::invalid_argument("Image must start with SOI marker");
    }

    sections_.reserve(10);

    SectionID marker_after_scan = SectionID::INVALID;

    while (true) {
        uint16_t marker_num = (marker_after_scan != SectionID::INVALID)
                                  ? static_cast<uint16_t>(marker_after_scan)
                                  : reader_.ReadDoubleByte();

        SectionID marker = DoubleByteToMarker(marker_num);

        if (marker == SectionID::INVALID) {
            throw std::invalid_argument("No such marker: `" + NumToHexString(marker_num) + "`");
        }

        if (marker == SectionID::SOI) {
            throw std::invalid_argument("Two SOI markers");
        }

        if (marker == SectionID::EOI) {
            break;
        }

        uint16_t length = reader_.ReadDoubleByte();

        if (length < 2) {
            throw std::invalid_argument("Size of marker must be >= 2");
        }

        DLOG(INFO) << "Met " << NumToHexString(marker_num) << " marker, size: " << length;

        sections_.emplace_back();
        sections_.back().reserve(length + 2);

        sections_.back().push_back((marker_num >> kBitsInByte));
        sections_.back().push_back(marker_num);

        sections_.back().push_back((length >> kBitsInByte));
        sections_.back().push_back(length);

        for (size_t i = 0; i + 2 < length; ++i) {
            sections_.back().push_back(reader_.ReadByte());
        }

        if (marker == SectionID::SOS) {
            // scan data is not measured
            while (true) {
                uint8_t byte = reader_.ReadByte();
                if (byte == 0xFF) {
                    uint16_t possible_marker_num =
                        (static_cast<uint16_t>(byte) << kBitsInByte) + reader_.ReadByte();
                    if (possible_marker_num != 0xFF00) {
                        marker_after_scan = DoubleByteToMarker(possible_marker_num);
                        if (marker_after_scan != SectionID::INVALID) {
                            break;
                        } else {
                            throw std::invalid_argument("No such marker: `" +
                                                        NumToHexString(marker_num) + "`");
                        }
                    }
                }
                sections_.back().push_back(byte);
            }
        }
    }

    DLOG(INFO) << "Separated markers successfully, start processing stages\n\n";

    MarkerOrderComparator comp(
        {{SectionID::SOF0, 0}, {SectionID::DHT, 1}, {SectionID::DQT, 2}, {SectionID::SOS, 3}});

    std::sort(sections_.begin(), sections_.end(),
              [&](const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs) {
                  return comp(DoubleByteToMarker((lhs[0] << kBitsInByte) + lhs[1]),
                              DoubleByteToMarker((rhs[0] << kBitsInByte) + rhs[1]));
              });

    MarkerFactory marker_processor;

    for (auto& bytes : sections_) {
        BitReader reader(&bytes);
        marker_processor.Handle(DoubleByteToMarker(reader.ReadDoubleByte()), reader, context_);
    }
}