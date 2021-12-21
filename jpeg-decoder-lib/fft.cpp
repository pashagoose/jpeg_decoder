#include "fft.h"

#include <glog/logging.h>

#include <fftw3.h>
#include <stdexcept>

DctCalculator::DctCalculator(size_t width, std::vector<double> *input, std::vector<double> *output)
    : input_(input), output_(output), width_(width) {
    if (!input || !output) {
        throw std::invalid_argument("No input or output given for IDCT");
    }

    if (input->size() % width != 0 || input->size() / width != width) {  // avoiding overflow
        throw std::invalid_argument("Input array is not WIDTHxWIDTH");
    }

    if (output->size() % width != 0 || output->size() / width != width) {
        throw std::invalid_argument("Output array is not WIDTHxWIDTH");
    }

    plan_ = fftw_plan_r2r_2d(width, width, input->data(), output->data(), FFTW_REDFT01,
                             FFTW_REDFT01, FFTW_ESTIMATE | FFTW_DESTROY_INPUT);
}

DctCalculator::~DctCalculator() {
    fftw_destroy_plan(plan_);
}

#include <iostream>

void DctCalculator::Inverse() {
    // DLOG(INFO) << "Calculating IDCT";

    (*input_)[0] *= 2.0;

    double sqrt_2 = sqrt(2.0);
    for (size_t i = 1; i < width_; ++i) {
        (*input_)[i] *= sqrt_2;
        (*input_)[i * width_] *= sqrt_2;
    }

    fftw_execute(plan_);

    double inverse_16 = 1.0 / 16.0;

    for (auto &val : *output_) {
        val *= inverse_16;
    }
}
