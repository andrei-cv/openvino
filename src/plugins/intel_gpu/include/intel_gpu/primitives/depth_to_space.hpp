// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "primitive.hpp"

namespace cldnn {

/// @brief mode for the @ref depth_to_space primitive.
enum class depth_to_space_mode : int32_t {
    /// @brief the input depth is divided to [block_size, ..., block_size, new_depth].
    blocks_first,
    /// @brief the input depth is divided to [new_depth, block_size, ..., block_size]
    depth_first
};

/// @brief
/// @details
struct depth_to_space : public primitive_base<depth_to_space> {
    CLDNN_DECLARE_PRIMITIVE(depth_to_space)

    /// @brief Constructs depth_to_space primitive.
    /// @param id This primitive id.
    /// @param input Input dictionary primitive id.
    /// @param block_size Block size.
    /// @param mode Depth division mode.
    depth_to_space(const primitive_id& id,
                   const input_info& input,
                   const size_t block_size,
                   const depth_to_space_mode mode,
                   const padding& output_padding = padding())
        : primitive_base(id, {input}, {output_padding})
        , block_size(block_size)
        , mode(mode) {}

    /// @brief Block size.
    size_t block_size;
    /// @brief depth division mode
    depth_to_space_mode mode;

    size_t hash() const override {
        size_t seed = primitive::hash();
        seed = hash_combine(seed, block_size);
        seed = hash_combine(seed, mode);
        return seed;
    }
};
}  // namespace cldnn
