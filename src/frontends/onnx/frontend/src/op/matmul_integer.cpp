// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "op/matmul_integer.hpp"

#include "openvino/op/constant.hpp"
#include "openvino/op/convert.hpp"
#include "openvino/op/matmul.hpp"
#include "openvino/op/subtract.hpp"
#include "openvino/op/unsqueeze.hpp"

using namespace ov::op;

OPENVINO_SUPPRESS_DEPRECATED_START
namespace ngraph {
namespace onnx_import {
namespace op {
namespace set_1 {
ov::OutputVector matmul_integer(const Node& node) {
    const ov::OutputVector& inputs = node.get_ng_inputs();

    const auto& A = inputs.at(0);
    const auto& B = inputs.at(1);
    const auto& A_zero_point = (inputs.size() > 2) ? inputs.at(2) : v0::Constant::create(ov::element::i32, {1}, {0});
    const auto& B_zero_point = (inputs.size() > 3) ? inputs.at(3) : v0::Constant::create(ov::element::i32, {1}, {0});

    const auto& converted_A = std::make_shared<v0::Convert>(A, ov::element::i32);
    const auto& converted_B = std::make_shared<v0::Convert>(B, ov::element::i32);

    const auto& converted_A_zero_point = std::make_shared<v0::Convert>(A_zero_point, ov::element::i32);
    const auto& converted_B_zero_point = std::make_shared<v0::Convert>(B_zero_point, ov::element::i32);

    const auto& A_zero_point_rank = A_zero_point.get_partial_shape().rank();

    ov::Output<ov::Node> shifted_A;
    if (A_zero_point_rank.is_static() && A_zero_point_rank.get_length() == 1) {
        const auto& one_node = v0::Constant::create(ov::element::i32, {1}, {1});
        const auto& reshaped_A_zero_point = std::make_shared<v0::Unsqueeze>(converted_A_zero_point, one_node);

        shifted_A = std::make_shared<v1::Subtract>(converted_A, reshaped_A_zero_point);
    } else {
        shifted_A = std::make_shared<v1::Subtract>(converted_A, converted_A_zero_point);
    }

    const auto& shifted_B = std::make_shared<v1::Subtract>(converted_B, converted_B_zero_point);

    const auto& result = std::make_shared<v0::MatMul>(shifted_A, shifted_B);

    return {result};
}
}  // namespace set_1
}  // namespace op
}  // namespace onnx_import
}  // namespace ngraph
OPENVINO_SUPPRESS_DEPRECATED_END
