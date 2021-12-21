#include "huffman.h"
#include <stdexcept>

#include <glog/logging.h>

void HuffmanTree::Build(const std::vector<uint8_t> &code_lengths,
                        const std::vector<uint8_t> &values) {
    // strong exception guarantee

    DLOG(INFO) << "Building Huffman tree";

    if (code_lengths.size() > kMaxTreeDepth) {
        throw std::invalid_argument("Huffman tree depth must not be greater than 16");
    }

    std::shared_ptr<Node> newroot = std::make_shared<Node>();
    std::shared_ptr<Node> current_node = newroot;
    std::vector<uint8_t> counter_lengths = code_lengths;

    size_t number_of_codes_left = 0;
    for (auto cnt : counter_lengths) {
        number_of_codes_left += cnt;
    }

    size_t depth = 0;
    if (number_of_codes_left) {
        // go left to first terminal
        while (depth < counter_lengths.size() + 1 &&
               (depth == 0 || counter_lengths[depth - 1] == 0)) {
            ++depth;
            current_node->left_son = std::make_shared<Node>(current_node);
            current_node = current_node->left_son;
        }

        if (values.empty()) {
            throw std::invalid_argument("Cannot build Huffman tree, not enough values");
        }

        --counter_lengths[depth - 1];
        --number_of_codes_left;

        current_node->value = values[0];
    }

    size_t index_val = 0;
    while (number_of_codes_left) {
        // lift up
        while (std::shared_ptr<Node> par = current_node->parent.lock()) {
            if (par->left_son == current_node) {
                current_node = par;
                --depth;
                break;
            }
            current_node = par;
            --depth;
        }

        if (!(current_node->parent.lock()) && current_node->right_son) {
            throw std::invalid_argument("Cannot build Huffman tree, incorrect structure");
        }

        // go one step right
        ++depth;
        current_node->right_son = std::make_shared<Node>(current_node);
        current_node = current_node->right_son;

        // go left until terminal
        while (depth < counter_lengths.size() + 1 && counter_lengths[depth - 1] == 0) {
            ++depth;
            current_node->left_son = std::make_shared<Node>(current_node);
            current_node = current_node->left_son;
        }

        if (depth == counter_lengths.size() + 1) {
            throw std::invalid_argument("Cannot build Huffman tree, incorrect structure");
        }

        // set value
        if (++index_val >= values.size()) {
            throw std::invalid_argument("Cannot build Huffman tree, not enough values");
        }

        --counter_lengths[depth - 1];
        --number_of_codes_left;
        current_node->value = values[index_val];
    }

    if (index_val + 1 != values.size() && index_val != 0) {
        throw std::invalid_argument("Cannot build Huffman tree, values array is too long");
    }

    root_ = newroot;
    current_vertex_ = root_;

    DLOG(INFO) << "Finished building Huffman tree";
}

bool HuffmanTree::Move(bool bit, int &value) {
    if (!current_vertex_) {
        throw std::invalid_argument("Cannot traverse unbuilt Huffman tree");
    }

    if ((!bit && !current_vertex_->left_son) || (bit && !current_vertex_->right_son)) {
        throw std::invalid_argument("No such code in Huffman tree");
    }

    if (bit) {
        current_vertex_ = current_vertex_->right_son;
    } else {
        current_vertex_ = current_vertex_->left_son;
    }

    if (current_vertex_->value) {
        value = *(current_vertex_->value);
        current_vertex_ = root_;
        return true;
    } else {
        return false;
    }
}
