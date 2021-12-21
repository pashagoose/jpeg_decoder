#include "marker_handlers.h"
#include <stdexcept>
#include <glog/logging.h>

void SectionDHT::Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) {
    DLOG(INFO) << "Processing DHT section";

    uint16_t size = reader.ReadDoubleByte();

    DLOG(INFO) << "Size: " << size;

    while (!reader.IsEnd()) {
        uint8_t type = reader.ReadHalfByte();
        uint8_t id = reader.ReadHalfByte();

        if (type != 0 && type != 1) {
            throw std::invalid_argument("First half byte should be 0 or 1 - AC or DC coefficient");
        }

        DLOG(INFO) << "Building DHT table: " << ((type == 1) ? "AC" : "DC")
                   << " coefficients, id: " << static_cast<size_t>(id);

        std::vector<uint8_t> code_lengths(HuffmanTree::kMaxTreeDepth);
        reader.FillVector(code_lengths);
        uint16_t values_count = std::reduce(code_lengths.begin(), code_lengths.end(), 0);
        std::vector<uint8_t> values(values_count);
        reader.FillVector(values);

        DLOG(INFO) << "Total values: " << values_count;

        std::unordered_map<uint8_t, HuffmanTree>& trees =
            (type == 1) ? context->ac_huffman_trees : context->dc_huffman_trees;

        if (trees.contains(id)) {
            throw std::invalid_argument("Section DHT overrides previous Huffman tree");
        }

        HuffmanTree tree;
        tree.Build(code_lengths, values);

        trees[id] = std::move(tree);
    }

    DLOG(INFO) << "Finished processing DHT section\n\n";
}

void SectionSOF0::Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) {
    DLOG(INFO) << "Processing SOF0 section";

    uint16_t size = reader.ReadDoubleByte();

    DLOG(INFO) << "Size: " << size;

    context->precision = reader.ReadByte();
    context->height = reader.ReadDoubleByte();
    context->width = reader.ReadDoubleByte();
    context->channels.resize(reader.ReadByte());
    // 8 bytes read by now

    DLOG(INFO) << "Precision: " << static_cast<size_t>(context->precision);
    DLOG(INFO) << "HxW: " << static_cast<size_t>(context->height) << ' '
               << static_cast<size_t>(context->width);
    DLOG(INFO) << "Channels: " << context->channels.size();

    if (!context->height || !context->width) {
        throw std::invalid_argument("Empty jpg");
    }

    if (context->precision != 8 && context->precision != 16) {
        throw std::invalid_argument("Precision is not 8, nor 16");
    }

    context->image.SetSize(context->width, context->height);

    uint8_t hmax = 0;
    uint8_t vmax = 0;

    DLOG(INFO) << "Channel thinning info: ";

    for (uint8_t i = 0; i < context->channels.size(); ++i) {
        uint8_t id = reader.ReadByte();
        --id;

        DLOG(INFO) << "Channel Id: " << static_cast<size_t>(id);

        context->channels[id].horizontal_thinning = reader.ReadHalfByte();
        context->channels[id].vertical_thinning = reader.ReadHalfByte();
        context->channels[id].qt_id = reader.ReadByte();

        DLOG(INFO) << "Horizontal: "
                   << static_cast<size_t>(context->channels[id].horizontal_thinning)
                   << " vertical: " << static_cast<size_t>(context->channels[id].vertical_thinning)
                   << " quantization table id: "
                   << static_cast<size_t>(context->channels[id].qt_id);

        hmax = std::max(hmax, context->channels[id].horizontal_thinning);
        vmax = std::max(vmax, context->channels[id].vertical_thinning);
    }

    for (size_t i = 0; i < context->channels.size(); ++i) {
        if (context->channels[i].horizontal_thinning == 0 ||
            context->channels[i].vertical_thinning == 0) {
            throw std::invalid_argument("Thinning coefficient must be not zero (cannot divide)");
        }

        context->channels[i].horizontal_thinning = hmax / context->channels[i].horizontal_thinning;
        context->channels[i].vertical_thinning = vmax / context->channels[i].vertical_thinning;

        if (context->channels[i].horizontal_thinning > 2 ||
            context->channels[i].vertical_thinning > 2) {
            throw std::invalid_argument("Horizonal or vertical thinning is > 2");
        }

        DLOG(INFO) << "Channel id: " << i << " horizontal thinning: "
                   << static_cast<size_t>(context->channels[i].horizontal_thinning)
                   << " vertical thinning: "
                   << static_cast<size_t>(context->channels[i].vertical_thinning);

        context->mcu_width = std::max(
            context->mcu_width,
            static_cast<uint8_t>(kDataUnitSide * context->channels[i].horizontal_thinning));

        context->mcu_height =
            std::max(context->mcu_height,
                     static_cast<uint8_t>(kDataUnitSide * context->channels[i].vertical_thinning));
    }

    DLOG(INFO) << "MCU parameters: " << static_cast<size_t>(context->mcu_height) << 'x'
               << static_cast<size_t>(context->mcu_width);

    if (size != 8 + context->channels.size() * 3) {
        throw std::invalid_argument("SOF0 section is too long");
    }

    DLOG(INFO) << "Finished processing SOF0 section\n\n";
}

void SectionCOM::Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) {
    DLOG(INFO) << "Processing COM section";

    uint16_t size = reader.ReadDoubleByte();

    std::string content(size - 2, 'a');
    reader.FillString(content);

    DLOG(INFO) << "Comment: " << content;

    context->image.SetComment(content);

    DLOG(INFO) << "Finished processing COM section\n\n";
}

void SectionSOS::Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) {
    DLOG(INFO) << "Processing SOS section";

    uint16_t sz = reader.ReadDoubleByte();
    uint8_t channels_count = reader.ReadByte();

    DLOG(INFO) << "Size: " << sz;
    DLOG(INFO) << "Channels: " << static_cast<size_t>(channels_count);

    if (channels_count != context->channels.size()) {
        throw std::invalid_argument("Different number of channels in SOF0 and SOS sections");
    }

    std::vector<HuffmanTree> channel_dc(channels_count), channel_ac(channels_count);
    std::vector<uint8_t> channel_ids;
    channel_ids.reserve(channels_count);

    for (size_t i = 0; i < channels_count; ++i) {
        uint8_t id_channel = reader.ReadByte();
        --id_channel;

        if (context->channels.size() <= id_channel) {
            throw std::invalid_argument("No such channel: `" + std::to_string(id_channel) + '`');
        }

        if (!channel_ids.empty() &&
            std::find(channel_ids.begin(), channel_ids.end(), id_channel) != channel_ids.end()) {
            throw std::invalid_argument("Channel description duplicate in SOS section");
        }

        channel_ids.emplace_back(id_channel);

        uint8_t dc_id = reader.ReadHalfByte();
        uint8_t ac_id = reader.ReadHalfByte();

        DLOG(INFO) << "Channel #" << static_cast<size_t>(id_channel)
                   << ", DC tree id: " << static_cast<size_t>(dc_id)
                   << ", AC tree id: " << static_cast<size_t>(ac_id);

        auto it_dc = context->dc_huffman_trees.find(dc_id);
        auto it_ac = context->ac_huffman_trees.find(ac_id);

        if (it_dc == context->dc_huffman_trees.end()) {
            throw std::invalid_argument("No DC Huffman tree with id: " + std::to_string(dc_id));
        }

        if (it_ac == context->ac_huffman_trees.end()) {
            throw std::invalid_argument("No AC Huffman tree with id: " + std::to_string(ac_id));
        }

        channel_dc[i] = it_dc->second;
        channel_ac[i] = it_ac->second;
    }

    if (channels_count * 2 + 6 != sz) {  // we read exactly channels_count * 2 + 3 bytes by now
        throw std::invalid_argument(
            "Incorrect size in SOS marker, should have exactly 3 bytes for progressive mode");
    }

    uint16_t mode_doublebyte = reader.ReadDoubleByte();
    uint8_t mode_byte = reader.ReadByte();

    if (mode_doublebyte != 0x3F || mode_byte) {
        throw std::invalid_argument("Can not read progressive jpg");
    }

    // prepare channels info

    std::vector<Channel> channels;
    channels.reserve(channels_count);

    for (uint8_t index : channel_ids) {
        channels.emplace_back(context->channels[index]);
    }

    context->channels = std::move(channels);

    // here we start huffman decoding

    auto mcu_it = context->GetMCUBeginIterator(std::move(channel_dc), std::move(channel_ac));

    while (!mcu_it.IsEnd()) {
        DLOG_EVERY_N(INFO, 100) << "Processing " << google::COUNTER << "th MCU out of "
                                << ((context->width + context->mcu_width - 1) /
                                    context->mcu_width) *
                                       ((context->height + context->mcu_height - 1) /
                                        context->mcu_height);

        mcu_it.Process(reader);

        ++mcu_it;
    }

    DLOG(INFO) << "Finished processing SOS section\n\n";
}

void SectionDQT::Process(BitReader<std::vector<uint8_t>>& reader, PictureContext* context) {
    DLOG(INFO) << "Started processing DQT section";

    uint16_t sz = reader.ReadDoubleByte();

    while (!reader.IsEnd()) {
        uint8_t value_sz = reader.ReadHalfByte();
        ++value_sz;
        uint8_t qt_id = reader.ReadHalfByte();

        DLOG(INFO) << "QTable #" << static_cast<size_t>(qt_id)
                   << ", value size: " << static_cast<size_t>(value_sz) << ", section size: " << sz;

        auto [it, inserted] = context->qts.emplace(qt_id, kDataUnitSide * kDataUnitSide);
        if (!inserted) {
            throw std::invalid_argument("Overriding existing QT, id: " + std::to_string(qt_id));
        }

        auto& table = it->second;
        DiagonalUnitIterator read_iter;

        while (!read_iter.IsEnd()) {
            auto [i, j] = read_iter.Get();
            table[i * kDataUnitSide + j] =
                (value_sz == 2) ? reader.ReadDoubleByte() : reader.ReadByte();
            read_iter.Proceed();
        }
    }

    DLOG(INFO) << "Finished processing DQT section\n\n";
}