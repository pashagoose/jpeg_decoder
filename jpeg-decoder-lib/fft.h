#pragma once

#include <fftw3.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

class DctCalculator {
public:
    // input and output are width by width matrices, first row, then
    // the second row.
    DctCalculator(size_t width, std::vector<double>* input, std::vector<double>* output);

    void Inverse();

    ~DctCalculator();

private:
    fftw_plan plan_;
    std::vector<double>* input_;
    std::vector<double>* output_;
    size_t width_;
};
