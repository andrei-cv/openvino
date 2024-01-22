// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "op/lp_norm.hpp"

#include "exceptions.hpp"
#include "openvino/core/validation_util.hpp"
#include "openvino/op/constant.hpp"
#include "openvino/op/divide.hpp"
#include "ov_models/ov_builders/norm.hpp"

using namespace ov::op;

OPENVINO_SUPPRESS_DEPRECATED_START
namespace ngraph {
namespace onnx_import {
namespace op {
namespace set_1 {
OutputVector lp_norm(const Node& node) {
    const Output<ov::Node> data{node.get_ng_inputs().at(0)};
    const auto data_shape = data.get_partial_shape();
    const auto data_rank = data_shape.rank();

    const std::int64_t p_norm{node.get_attribute_value<std::int64_t>("p", 2)};

    const std::int64_t axis{node.get_attribute_value<std::int64_t>("axis", -1)};
    OPENVINO_SUPPRESS_DEPRECATED_START
    const size_t normalize_axis = ov::normalize_axis(node.get_description(), axis, data_rank);
    OPENVINO_SUPPRESS_DEPRECATED_END

    CHECK_VALID_NODE(node,
                     p_norm == 1 || p_norm == 2,
                     "Invalid `p` attribute value: ",
                     p_norm,
                     "Only normalization of 1st or 2nd order is supported.");

    const auto normalize_axis_const = v0::Constant::create(element::i64, {}, {normalize_axis});
    std::shared_ptr<ov::Node> norm =
        ov::op::util::lp_norm(data, normalize_axis_const, static_cast<std::size_t>(p_norm), 0.0f, true);

    return {std::make_shared<v1::Divide>(data, norm)};
}

}  // namespace set_1

}  // namespace op

}  // namespace onnx_import

}  // namespace ngraph
OPENVINO_SUPPRESS_DEPRECATED_END
