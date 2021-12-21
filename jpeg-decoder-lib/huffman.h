#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

// HuffmanTree decoder for DHT section.
class HuffmanTree {
public:
    constexpr static inline size_t kMaxTreeDepth = 16;
    // code_lengths is the array of size no more than 16 with number of
    // terminated nodes in the Huffman tree.
    // values are the values of the terminated nodes in the consecutive
    // level order.
    void Build(const std::vector<uint8_t>& code_lengths, const std::vector<uint8_t>& values);

    // Moves the state of the huffman tree by |bit|. If the node is terminated,
    // returns true and overwrites |value|. If it is intermediate, returns false
    // and value is unmodified.
    bool Move(bool bit, int& value);

private:
    struct Node {
        std::optional<uint8_t> value;
        std::shared_ptr<Node> left_son;
        std::shared_ptr<Node> right_son;
        std::weak_ptr<Node> parent;

        Node() = default;
        Node(std::weak_ptr<Node> par) : parent(par) {
        }
    };

    std::shared_ptr<Node> root_;
    std::shared_ptr<Node> current_vertex_;
};
