// Copyright (C) 2018-2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/reference/random_uniform.hpp"

#include <ctime>
#include <memory>

#include "openvino/core/except.hpp"
#include "openvino/core/shape.hpp"
#include "openvino/reference/utils/phillox_converter.hpp"
#include "openvino/reference/utils/phillox_generator.hpp"

namespace ov {
namespace reference {

// Implementation of RandomUniform that uses Philox algorithm as inner random unsigned integer generator.
std::pair<uint64_t, uint64_t> random_uniform(const uint64_t* out_shape,
                                             const char* min_val,
                                             const char* max_val,
                                             char* out,
                                             const Shape& out_shape_shape,
                                             const element::Type& elem_type,
                                             uint64_t seed,
                                             uint64_t seed2,
                                             std::pair<uint64_t, uint64_t> prev_state,
                                             ov::op::PhilloxAlignment alignment) {
    // When both seed values are equal to zero RandomUniform should generate non-deterministic sequence.
    // Implementation in plugins may differ for this case.
    if (seed == 0 && seed2 == 0) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        seed = std::rand();
    }

    // Calculate total element count for generation
    size_t shape_count = shape_size(out_shape_shape);
    size_t elem_count = 1;
    for (size_t i = 0; i < shape_count; i++) {
        elem_count *= out_shape[i];
    }

    // Sets up the generator of random numbers and matching converter
    std::shared_ptr<phillox::PhilloxGenerator> generator =
        phillox::make_phillox_generator(seed, seed2, prev_state, elem_count, alignment);
    std::shared_ptr<phillox::PhilloxConverter> converter =
        phillox::make_phillox_converter(out, elem_type, elem_count, min_val, max_val, alignment);

    // Generate randon numbers and convert them until the output array is full
    const size_t step = converter->get_converted_elements_count();
    for (size_t i = 0; i < elem_count; i += step) {
        phillox::PhilloxOutput result = generator->random();
        converter->convert(result, i);
    }

    // Return the next state to feed into the generator
    return generator->get_next_state();
}

}  // namespace reference
}  // namespace ov
