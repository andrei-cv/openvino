// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "shared_test_classes/base/utils/generate_inputs.hpp"

#include "openvino/op/ops.hpp"
#include "ov_ops/augru_cell.hpp"
#include "ov_ops/augru_sequence.hpp"

#include "common_test_utils/ov_tensor_utils.hpp"
#include "common_test_utils/data_utils.hpp"

#include "shared_test_classes/base/ov_subgraph.hpp"
#include "shared_test_classes/single_op/roi_align.hpp"

#include "ov_models/utils/data_utils.hpp"

namespace ov {
namespace test {
namespace utils {

namespace {
struct {
    double max = 0;
    double min = 0;
    bool is_defined = false;
} const_range;
}  // namespace

void set_const_ranges(double _min, double _max) {
    const_range.max = _max;
    const_range.min = _min;
    const_range.is_defined = true;
}

void reset_const_ranges() {
    const_range.is_defined = false;
}

std::vector<uint8_t> color_test_image(size_t height, size_t width, int b_step, ov::preprocess::ColorFormat format) {
    // Test all possible r/g/b values within dimensions
    int b_dim = 255 / b_step + 1;
    auto input_yuv = std::vector<uint8_t>(height * b_dim * width * 3 / 2);
    for (int b = 0; b <= 255; b += b_step) {
        for (size_t y = 0; y < height / 2; y++) {
            for (size_t x = 0; x < width / 2; x++) {
                int r = static_cast<int>(y) * 512 / static_cast<int>(height);
                int g = static_cast<int>(x) * 512 / static_cast<int>(width);
                // Can't use random y/u/v for testing as this can lead to invalid R/G/B values
                int y_val = ((66 * r + 129 * g + 25 * b + 128) / 256) + 16;
                int u_val = ((-38 * r - 74 * g + 112 * b + 128) / 256) + 128;
                int v_val = ((112 * r - 94 * g + 18 * b + 128) / 256) + 128;

                size_t b_offset = height * width * b / b_step * 3 / 2;
                if (ov::preprocess::ColorFormat::I420_SINGLE_PLANE == format ||
                    ov::preprocess::ColorFormat::I420_THREE_PLANES == format) {
                    size_t u_index = b_offset + height * width + y * width / 2 + x;
                    size_t v_index = u_index + height * width / 4;
                    input_yuv[u_index] = u_val;
                    input_yuv[v_index] = v_val;
                } else {
                    size_t uv_index = b_offset + height * width + y * width + x * 2;
                    input_yuv[uv_index] = u_val;
                    input_yuv[uv_index + 1] = v_val;
                }
                size_t y_index = b_offset + y * 2 * width + x * 2;
                input_yuv[y_index] = y_val;
                input_yuv[y_index + 1] = y_val;
                input_yuv[y_index + width] = y_val;
                input_yuv[y_index + width + 1] = y_val;
            }
        }
    }
    return input_yuv;
}

namespace {

/**
 * Sets proper range and resolution for real numbers generation
 *
 * range = 8 and resolution 32
 *
 * The worst case scenario is 7 + 31/32 (7.96875)
 * IEEE 754 representation is:
 * ----------------------------------------------
 *      sign | exponent | mantissa
 * ----------------------------------------------
 * FP32    0 | 10000001 | 11111110000000000000000
 * FP16    0 |    10001 | 1111111000
 * BF16    0 | 10000001 | 1111111
 * ----------------------------------------------
 *
 * All the generated numbers completely fit into the data type without truncation
 */

using ov::test::utils::InputGenerateData;


static inline void set_real_number_generation_data(InputGenerateData& inGenData) {
    inGenData.range = 8;
    inGenData.resolution = 32;
}

ov::Tensor generate(const std::shared_ptr<ov::Node>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    InputGenerateData inGenData;

    if (const_range.is_defined) {
        auto min_orig = inGenData.start_from;
        auto max_orig = inGenData.start_from + inGenData.range * inGenData.resolution;
        auto min_ref = const_range.min;
        auto max_ref = const_range.max;
        if (min_orig < min_ref || min_orig == 0)
            inGenData.start_from = min_ref;
        inGenData.range = (max_orig > max_ref || max_orig == 10 ? max_ref : max_orig - inGenData.start_from) - inGenData.start_from;
    }

    if (elemType.is_real()) {
        set_real_number_generation_data(inGenData);
    }

    const size_t inNodeCnt = node->get_input_size();
    auto it = inputRanges.find(node->get_type_info());
    if (it != inputRanges.end()) {
        const auto& ranges = it->second;
        if (ranges.size() != 2) {
            throw std::runtime_error("Incorrect size of ranges. It should be 2 (real and int cases)");
        }
        const auto& range = ranges.at(elemType.is_real());
        inGenData = range.size() < inNodeCnt ? range.front() : range.at(port);
    }
    return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
}

namespace Activation {
ov::Tensor generate(const ov::element::Type& elemType,
                             const ov::Shape& targetShape,
                             InputGenerateData inGenData = InputGenerateData(-1, 2, 32768, 1)) {
    if (!elemType.is_signed()) {
        inGenData.range = 15;
        inGenData.start_from = 0;
    }
    return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
}
} // namespace Activation

ov::Tensor generate(const std::shared_ptr<ov::op::v0::HardSigmoid>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    switch (port) {
        case 1: {
            ov::test::utils::InputGenerateData in_data;
            in_data.start_from = 0.2;
            in_data.range = 0;
            return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_data);
        }
        case 2: {
            ov::test::utils::InputGenerateData in_data;
            in_data.start_from = 0.5;
            in_data.range = 0;
            return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_data);
        }
        default: {
            return Activation::generate(elemType, targetShape);
        }
    }

    return Activation::generate(elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v0::PRelu>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    switch (port) {
        case 1: {
            auto name = node->input(1).get_node()->get_friendly_name();
            if (0 == name.compare("leakySlope")) {
                ov::test::utils::InputGenerateData in_data;
                in_data.start_from = 0.01;
                in_data.range = 0;
                return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_data);
            } else if (0 == name.compare("negativeSlope")) {
                ov::test::utils::InputGenerateData in_data;
                in_data.start_from = -0.01;
                in_data.range = 0;
                return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_data);
            } else {
                return Activation::generate(elemType, targetShape);
            }
        }
        default: {
            return Activation::generate(elemType, targetShape);
        }
    }
}

ov::Tensor generate(const std::shared_ptr<ov::op::v0::Selu>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    switch (port) {
        case 1: {
            std::vector<float> alpha(node->get_input_shape(1).size(), 1.6732f);
            return ov::test::utils::create_tensor<float>(elemType, targetShape, alpha, alpha.size());
        }
        case 2: {
            std::vector<float> lambda(node->get_input_shape(2).size(), 1.0507f);
            return ov::test::utils::create_tensor<float>(elemType, targetShape, lambda, lambda.size());
        }
        default: {
            return Activation::generate(elemType, targetShape);
        }
    }
}

ov::Tensor generate(const std::shared_ptr<ov::op::v0::DetectionOutput>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    InputGenerateData inGenData;
    inGenData.start_from = 0;
    inGenData.range = 1;

    switch (port) {
        case 1:
        case 3:
            inGenData.resolution = 1000;
            break;
        case 2:
            if (node->get_attrs().normalized) {
                inGenData.resolution = 1000;
            } else {
                inGenData.range = 10;
            }
            break;
        default:
            inGenData.resolution = 10;
            break;
    }
    return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v0::FakeQuantize>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    int seed = 1;
    size_t constDataSize = ov::shape_size(targetShape);
    std::vector<float> inputLowData, inputHighData, outputLowData, outputHighData;
    inputLowData = NGraphFunctions::Utils::generateVector<ov::element::f32>(constDataSize, 10, 1, seed);
    if (node->get_levels() != 2) {
        inputHighData = NGraphFunctions::Utils::generateVector<ov::element::f32>(constDataSize, 10, 1, seed);
        outputLowData = NGraphFunctions::Utils::generateVector<ov::element::f32>(constDataSize, 10, 1, seed);
        outputHighData = NGraphFunctions::Utils::generateVector<ov::element::f32>(constDataSize, 10, 1, seed);
    } else {
        inputHighData = inputLowData;
        outputLowData = NGraphFunctions::Utils::generateVector<ov::element::f32>(constDataSize, 10, 1, seed);
        outputHighData = NGraphFunctions::Utils::generateVector<ov::element::f32>(constDataSize, 10, 1, seed);

        for (int i = 0; i < constDataSize; i++) {
            if (outputLowData[i] > outputHighData[i]) {
                outputLowData[i] = 1;
                outputHighData[i] = 0;
            } else {
                outputLowData[i] = 0;
                outputHighData[i] = 1;
            }
        }
    }

    for (int i = 0; i < constDataSize; i++) {
        inputLowData[i] = std::min(inputLowData[i], inputHighData[i]);
        inputHighData[i] = std::max(inputLowData[i], inputHighData[i]);
        if (inputLowData[i] == inputHighData[i])
            inputHighData[i] += 1;
    }

    for (int i = 0; i < constDataSize; i++) {
        outputLowData[i] = std::min(outputLowData[i], outputHighData[i]);
        outputHighData[i] = std::max(outputLowData[i], outputHighData[i]);
        if (outputLowData[i] == outputHighData[i])
            outputHighData[i] += 1;
    }
    switch (port) {
        case 1:
            return ov::test::utils::create_tensor<float>(elemType, targetShape, inputLowData, inputLowData.size());
        case 2:
            return ov::test::utils::create_tensor<float>(elemType, targetShape, inputHighData, inputHighData.size());
        case 3:
            return ov::test::utils::create_tensor<float>(elemType, targetShape, outputLowData, outputLowData.size());
        case 4:
            return ov::test::utils::create_tensor<float>(elemType, targetShape, outputHighData, outputHighData.size());
        default: {
            InputGenerateData inGenData;
            inGenData.range = 10.f;
            inGenData.resolution = 1.0f;
            inGenData.seed = seed;

            return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
        }
    }
}

ov::Tensor generate(const std::shared_ptr<ov::op::v0::PSROIPooling>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    const auto &inputShape = node->get_input_shape(0);
    if (port == 1) {
        auto tensor = ov::Tensor(elemType, targetShape);
#define CASE(X) case X: ::ov::test::utils::fill_psroi(tensor,       \
                                    inputShape[0],                  \
                                    inputShape[2],                  \
                                    inputShape[3],                  \
                                    node->get_group_size(),         \
                                    node->get_spatial_scale(),      \
                                    node->get_spatial_bins_x(),     \
                                    node->get_spatial_bins_y(),     \
                                    node->get_mode()); break;       \

    switch (elemType) {
        CASE(ov::element::Type_t::f16)
        CASE(ov::element::Type_t::f32)
        default: OPENVINO_THROW("Unsupported element type: ", elemType);
    }
#undef CASE
        return tensor;
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v0::ROIPooling>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 1) {
        const auto &inputShape = node->get_input_shape(0);
        auto tensor = ov::Tensor(elemType, targetShape);
#define CASE(X) case X: ::ov::test::utils::fill_data_roi(                   \
    tensor,                                                                 \
    node->get_input_shape(0).front() - 1,                                   \
    inputShape[2],                                                          \
    inputShape[3],                                                          \
    1.0f,                                                                   \
    node->get_method() == "max"); break;                                    \

    switch (elemType) {
        CASE(ov::element::Type_t::boolean)
        CASE(ov::element::Type_t::i8)
        CASE(ov::element::Type_t::i16)
        CASE(ov::element::Type_t::i32)
        CASE(ov::element::Type_t::i64)
        CASE(ov::element::Type_t::u8)
        CASE(ov::element::Type_t::u16)
        CASE(ov::element::Type_t::u32)
        CASE(ov::element::Type_t::u64)
        CASE(ov::element::Type_t::bf16)
        CASE(ov::element::Type_t::f16)
        CASE(ov::element::Type_t::f32)
        CASE(ov::element::Type_t::f64)
        CASE(ov::element::Type_t::u1)
        CASE(ov::element::Type_t::i4)
        CASE(ov::element::Type_t::u4)
        default: OPENVINO_THROW("Unsupported element type: ", elemType);
    }
#undef CASE
        return tensor;
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}


ov::Tensor generate(const std::shared_ptr<ov::op::v1::GatherTree>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    auto &shape = node->get_input_shape(0);
    auto maxBeamIndx = shape.at(2) - 1;

    switch (port) {
        case 2:
        case 3: {
            InputGenerateData inGenData;
            inGenData.start_from = maxBeamIndx / 2;
            inGenData.range = maxBeamIndx;
            return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
        }
        default:
            InputGenerateData inGenData;
            inGenData.range = maxBeamIndx;
            return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
    }
}

namespace LogicalOp {
ov::Tensor generate(const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    ov::test::utils::InputGenerateData in_data;
    in_data.start_from = 0;
    in_data.range = 2;
    return create_and_fill_tensor(elemType, targetShape, in_data);
}
}

ov::Tensor generate(const std::shared_ptr<ov::op::v1::LogicalAnd>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    return LogicalOp::generate(elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v1::LogicalNot>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    return LogicalOp::generate(elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v1::LogicalOr>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    return LogicalOp::generate(elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v1::LogicalXor>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    return LogicalOp::generate(elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v1::ReduceLogicalAnd>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    return LogicalOp::generate(elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v1::ReduceLogicalOr>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    return LogicalOp::generate(elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v3::Bucketize>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    InferenceEngine::Blob::Ptr blobPtr;
    switch (port) {
        case 0: {
            auto data_size = shape_size(targetShape);
            ov::test::utils::InputGenerateData in_data;
            in_data.start_from = 0;
            in_data.range = data_size * 5;
            in_data.resolution = 10;
            in_data.seed = 7235346;
            return create_and_fill_tensor(elemType, targetShape, in_data);
        }
        case 1: {
            return  create_and_fill_tensor_unique_sequence(elemType, targetShape, 0, 10, 8234231);
        }
        default:
            return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
    }
}

ov::Tensor generate(const std::shared_ptr<ov::op::v3::ROIAlign>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    switch (port) {
        case 1: {
            if (node->get_sampling_ratio() != 0) {
                const auto &inputShape = node->get_input_shape(0);
                std::vector<float> blobData(node->get_shape()[0] * 4);
                ov::test::ROIAlignLayerTest::fillCoordTensor(blobData,
                                                             inputShape[2],
                                                             inputShape[3],
                                                             node->get_spatial_scale(),
                                                             node->get_sampling_ratio(),
                                                             node->get_pooled_h(),
                                                             node->get_pooled_w());
                return ov::test::utils::create_tensor<float>(ov::element::f32, targetShape, blobData);
            } else {
                return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
            }
        }
        case 2: {
            std::vector<int> roiIdxVector(node->get_shape()[0]);
            ov::test::ROIAlignLayerTest::fillIdxTensor(roiIdxVector, node->get_shape()[0]);
            return ov::test::utils::create_tensor<int>(elemType, targetShape, roiIdxVector);
        }
        default:
            return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
    }
}

ov::Tensor generate(const std::shared_ptr<ov::op::v4::Proposal>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 1) {
        return ov::test::utils::create_and_fill_tensor_normal_distribution(elemType, targetShape, 0.0f, 0.2f, 7235346);
    } else if (port == 2) {
        ov::Tensor tensor = ov::Tensor(elemType, targetShape);

        auto *dataPtr = tensor.data<float>();
        dataPtr[0] = dataPtr[1] = 225.0f;
        dataPtr[2] = 1.0f;
        if (tensor.get_size() == 4)
            dataPtr[3] = 1.0f;

        return tensor;
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v5::BatchNormInference>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    return ov::test::utils::create_and_fill_tensor_consistently(elemType, targetShape, 3, 0, 1);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v5::GRUSequence>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 2) {
        ov::test::utils::InputGenerateData in_data;
        in_data.start_from = 0;
        in_data.range = 10; // max_seq_len
        return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_data);
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v5::LSTMSequence>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 2) {
        ov::test::utils::InputGenerateData in_data;
        in_data.start_from = 0;
        in_data.range = 10; // max_seq_len
        return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_data);
    }
    if (port == 3 && node->input(0).get_partial_shape().is_static()) {
        ov::test::utils::InputGenerateData in_data;
        in_data.start_from = 0;
        in_data.range = node->input(0).get_shape()[1]; // seq_len
        return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_data);
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v3::EmbeddingSegmentsSum>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 2) {
        ov::Tensor tensor = ov::Tensor(elemType, targetShape);

        const auto &outputShape = node->get_output_shape(0);
        const size_t range = outputShape[0] - 1; // values in segmentsIds should be less than num_segments
        const size_t startFrom = 0;
        const int seed = 1;
        std::default_random_engine random(seed);
        switch (elemType) {
            case element::Type_t::i32: {
                std::uniform_int_distribution<int32_t> distribution(startFrom, (startFrom + range));

                auto *dataPtr = tensor.data<int32_t>();
                for (size_t i = 0; i < tensor.get_size(); i++) {
                    dataPtr[i] = distribution(random);
                }
                return tensor;
            }
            case element::Type_t::i64: {
                std::uniform_int_distribution<int64_t> distribution(startFrom, (startFrom + range));

                auto *dataPtr = tensor.data<int64_t>();
                for (size_t i = 0; i < tensor.get_size(); i++) {
                    dataPtr[i] = distribution(random);
                }
                return tensor;
            }
            default:
                OPENVINO_THROW("Unsupported element type for segment_ids: ", elemType);
        }
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::internal::AUGRUSequence>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 6) {
        ov::Tensor tensor = ov::Tensor(elemType, targetShape);

        const size_t range = 1;
        const size_t startFrom = 0;
        const size_t k = 1000;
        const int seed = 1;
        std::default_random_engine random(seed);
        std::uniform_int_distribution<int32_t> distribution(k * startFrom, k * (startFrom + range));

        auto *dataPtr = tensor.data<float>();
        for (size_t i = 0; i < tensor.get_size(); i++) {
            auto value = static_cast<float>(distribution(random));
            dataPtr[i] = value / static_cast<float>(k);
        }
        return tensor;
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::internal::AUGRUCell>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 5) {
        ov::Tensor tensor = ov::Tensor(elemType, targetShape);

        const size_t range = 1;
        const size_t startFrom = 0;
        const size_t k = 1000;
        const int seed = 1;
        std::default_random_engine random(seed);
        std::uniform_int_distribution<int32_t> distribution(k * startFrom, k * (startFrom + range));

        auto *dataPtr = tensor.data<float>();
        for (size_t i = 0; i < tensor.get_size(); i++) {
            auto value = static_cast<float>(distribution(random));
            dataPtr[i] = value / static_cast<float>(k);
        }
        return tensor;
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

template<ov::element::Type_t elemType>
ov::Tensor generate_unique_possibilities(const ov::Shape &targetShape) {
    using value_type = typename element_type_traits<elemType>::value_type;
    ov::Tensor tensor = ov::Tensor(elemType, targetShape);
    const size_t k = targetShape[0];
    std::vector<size_t> indices(k);
    std::iota(indices.begin(), indices.end(), 0lu);
    std::default_random_engine random;
    std::shuffle(indices.begin(), indices.end(), random);

    auto dataPtr = tensor.data<value_type>();
    for (size_t i = 0; i < k; ++i) {
        // our goal is to have unique values for both f32 and f16 to avoid false failures because of the same possibilities
        dataPtr[i] = ov::float16::from_bits(static_cast<  uint16_t>(indices[i]));
    }
    return tensor;
}

ov::Tensor generate(const std::shared_ptr<ov::op::v6::ExperimentalDetectronTopKROIs>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 1) {
        switch (elemType) {
            case element::Type_t::f16:
                return generate_unique_possibilities<element::Type_t::f16>(targetShape);
            case element::Type_t::f32:
                return generate_unique_possibilities<element::Type_t::f32>(targetShape);
            default:
                OPENVINO_THROW("Unsupported element type: ", elemType);
        }
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v5::RNNSequence>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 2) {
        ov::test::utils::InputGenerateData in_data;
        in_data.start_from = 0;
        in_data.range = 10; // max_seq_len
        return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_data);
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const std::shared_ptr<ov::op::v8::Softmax>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    auto axis = node->get_axis();
    axis = axis < 0 ? targetShape.size() + axis : axis;
    unsigned datasetSize = std::accumulate(targetShape.begin() + axis, targetShape.end(), 1,
        [](std::size_t a, size_t b) { return a * b; });
    // Generate small negative values for datasets which exceed 2048 size
    // to avoid NaN values in Softmax results for fp16 precision
    if (datasetSize >= 2048 && static_cast<ov::element::Type_t>(elemType) == ov::element::Type_t::f16)
        return ov::test::utils::create_and_fill_tensor_normal_distribution(elemType, targetShape, -5.f, 0.5f, 7235346);
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v1::DeformablePSROIPooling>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 1) {
        ov::Tensor tensor(elemType, targetShape);
        auto data_input_shape = node->input(0).get_shape();
        const auto batch_distrib = data_input_shape[0] - 1;
        const auto height = data_input_shape[2] / node->get_spatial_scale();
        const auto width  = data_input_shape[3] / node->get_spatial_scale();

        ov::test::utils::fill_data_roi(tensor, batch_distrib, height, width, 1.0f, true);
        return tensor;
    } else if (port == 2) {
        ov::Tensor tensor(elemType, targetShape);
        ov::test::utils::fill_tensor_random(tensor, 1.8, -0.9);
        return tensor;
    }
    return generate(std::static_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v3::ScatterNDUpdate>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    // when fill indices
    if (port == 1) {
        auto srcShape = node->get_input_shape(0);
        // the data in indices must be unique.
        // so need to select part data from total collection
        // Calculate the collection size
        int k = targetShape[targetShape.size() - 1];
        int totalSize = 1;
        for (int i = 0; i < k; i++) {
            totalSize *= srcShape[i];
        }
        size_t indShapeSize = ov::shape_size(targetShape);
        // Calculate the size of part data
        int selectNums = indShapeSize / k;
        // create total collection
        std::vector<int> collection(totalSize);
        for (int i = 0; i < totalSize; i++) {
            collection[i] = i;
        }
        // select part data from collection
        // the last selectNums data in collection are what want to be filled into tensor
        testing::internal::Random random(1);
        int r = 0;
        int tmp = 0;
        for (int i = 0, y = totalSize; i < selectNums; i++, y--) {
            r = random.Generate(y);
            // switch y and r
            tmp = collection[y - 1];
            collection[y - 1] = collection[r];
            collection[r] = tmp;
        }
        // if the shape of source data is (a ,b ,c)
        // the strides is (bc, c, 1)
        std::vector<int> strides;
        int stride = 1;
        strides.push_back(stride);
        for (int i = k - 1; i > 0; i--) {
            stride *= srcShape[i];
            strides.push_back(stride);
        }
        std::reverse(strides.begin(), strides.end());
        // create tensor and fill function
        auto tensor = ov::Tensor{elemType, targetShape};
        auto fill_data = [&elemType, &tensor](int offset, int value) {
            switch (elemType) {
                case ov::element::Type_t::i32: {
                    auto data =
                        tensor.data<element_type_traits<ov::element::Type_t::i32>::value_type>();
                    data[offset] = value;
                    break;
                }
                case ov::element::Type_t::i64: {
                    auto data =
                        tensor.data<element_type_traits<ov::element::Type_t::i64>::value_type>();
                    data[offset] = value;
                    break;
                }
                default:
                    throw std::runtime_error("indices type should be int32 or int64");
            }
        };
        // start to fill data
        int index = 0;
        int tmpNum = 0;
        for (int i = totalSize - selectNums, y = 0; i < totalSize; i++, y = y + k) {
            tmpNum = collection[i];
            for (int z = 0; z < k; z++) {
                //Calculate index of dims
                index = tmpNum / strides[z];
                tmpNum = tmpNum % strides[z];
                fill_data(y + z, index);
            }
        }
        return tensor;
    } else {
        return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType,
                        targetShape);
    }
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v1::TopK>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    auto tensor = ov::Tensor{elemType, targetShape};
    size_t size = tensor.get_size();
    int start = - static_cast<int>(size / 2);
    std::vector<int> data(size);
    std::iota(data.begin(), data.end(), start);
    std::mt19937 gen(0);
    std::shuffle(data.begin(), data.end(), gen);

    float divisor = size / 10.0;

    if (tensor.get_element_type() == ov::element::f32) {
        auto *p = tensor.data<float>();
        for (size_t i = 0; i < size; i++)
            p[i] = static_cast<float>(data[i] / divisor);
    } else if (tensor.get_element_type() == ov::element::f16) {
        auto *p = tensor.data<ov::float16>();
        for (size_t i = 0; i < size; i++)
            p[i] = static_cast<ov::float16>(data[i] / divisor);
    } else {
        OPENVINO_THROW("Unsupported element type: ", tensor.get_element_type());
    }
    return tensor;
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v8::DeformableConvolution>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    InputGenerateData in_gen_data;
    if (elemType.is_real()) {
        set_real_number_generation_data(in_gen_data);
    }

    if (1 == port) {
        in_gen_data.range = 2;
        in_gen_data.start_from = 0;
        in_gen_data.resolution = 10;
    } else if (2 == port) {
        in_gen_data.range = 1;
        in_gen_data.start_from = 0;
        in_gen_data.resolution = 20;
    }
    return ov::test::utils::create_and_fill_tensor(elemType, targetShape, in_gen_data);
}

namespace comparison {
void fill_tensor(ov::Tensor& tensor) {
    auto data_ptr = static_cast<float*>(tensor.data());
    auto data_ptr_int = static_cast<int*>(tensor.data());
    auto range = tensor.get_size();
    auto start = -static_cast<float>(range) / 2.f;
    testing::internal::Random random(1);
    for (size_t i = 0; i < range; i++) {
        if (i % 7 == 0) {
            data_ptr[i] = std::numeric_limits<float>::infinity();
        } else if (i % 7 == 1) {
            data_ptr[i] = -std::numeric_limits<float>::infinity();
        } else if (i % 7 == 2) {
            data_ptr_int[i] = 0x7F800000 + random.Generate(range);
        } else if (i % 7 == 3) {
            data_ptr[i] = std::numeric_limits<double>::quiet_NaN();
        } else if (i % 7 == 5) {
            data_ptr[i] = -std::numeric_limits<double>::quiet_NaN();
        } else {
            data_ptr[i] = start + static_cast<float>(random.Generate(range));
        }
    }
}
} // namespace comparison

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v10::IsFinite>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    ov::Tensor tensor(elemType, targetShape);
    comparison::fill_tensor(tensor);
    return tensor;
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v10::IsNaN>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    ov::Tensor tensor{elemType, targetShape};
    comparison::fill_tensor(tensor);
    return tensor;
}

namespace is_inf {
template <typename T>
void fill_tensor(ov::Tensor& tensor) {
    int range = ov::shape_size(tensor.get_shape());
    float startFrom = -static_cast<float>(range) / 2.f;

    auto pointer = tensor.data<T>();
    testing::internal::Random random(1);
    for (size_t i = 0; i < range; i++) {
        if (i % 7 == 0) {
            pointer[i] = std::numeric_limits<T>::infinity();
        } else if (i % 7 == 1) {
            pointer[i] = std::numeric_limits<T>::quiet_NaN();
        } else if (i % 7 == 3) {
            pointer[i] = -std::numeric_limits<T>::infinity();
        } else if (i % 7 == 5) {
            pointer[i] = -std::numeric_limits<T>::quiet_NaN();
        } else {
            pointer[i] = static_cast<T>(startFrom + random.Generate(range));
        }
    }
}
} // namespace is_inf

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v10::IsInf>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    auto tensor = ov::Tensor(elemType, targetShape);
    if (elemType == ov::element::f16) {
        is_inf::fill_tensor<ov::float16>(tensor);
    } else {
        is_inf::fill_tensor<float>(tensor);
    }
    return tensor;
}

namespace color_conversion {
void fill_tensor(ov::Tensor& tensor, ov::preprocess::ColorFormat format) {
    size_t full_height = tensor.get_shape()[1];
    size_t full_width = tensor.get_shape()[2];
    int b_dim = static_cast<int>(full_height * 2 / (3 * full_width));
    ASSERT_GT(b_dim, 1) << "Image height is invalid";
    ASSERT_EQ(255 % (b_dim - 1), 0) << "Image height is invalid";
    int b_step = 255 / (b_dim - 1);
    auto input_image = color_test_image(full_width, full_width, b_step, format);
    auto data_ptr = static_cast<uint8_t*>(tensor.data());
    for (size_t j = 0; j < input_image.size(); ++j) {
        data_ptr[j] = input_image[j];
    }
}
} // namespace color_conversion

ov::Tensor generate(const std::shared_ptr<ov::op::v8::I420toRGB>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    auto b_dim = static_cast<int>(targetShape[1] * 2 / (3 * targetShape[2]));
    if (node->inputs().size() > 1 || b_dim < 2)
        return generate(std::static_pointer_cast<ov::Node>(node), port, elemType, targetShape);
    ov::Tensor tensor(elemType, targetShape);
    color_conversion::fill_tensor(tensor, ov::preprocess::ColorFormat::I420_SINGLE_PLANE);
    return tensor;
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v8::I420toBGR>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    auto b_dim = static_cast<int>(targetShape[1] * 2 / (3 * targetShape[2]));
    if (node->inputs().size() > 1 || b_dim < 2)
        return generate(std::static_pointer_cast<ov::Node>(node), port, elemType, targetShape);
    ov::Tensor tensor(elemType, targetShape);
    color_conversion::fill_tensor(tensor, ov::preprocess::ColorFormat::I420_SINGLE_PLANE);
    return tensor;
}


ov::Tensor generate(const
                             std::shared_ptr<ov::op::v8::NV12toRGB>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    auto b_dim = static_cast<int>(targetShape[1] * 2 / (3 * targetShape[2]));
    if (node->inputs().size() > 1 || b_dim < 2)
        return generate(std::static_pointer_cast<ov::Node>(node), port, elemType, targetShape);
    ov::Tensor tensor(elemType, targetShape);
    color_conversion::fill_tensor(tensor, ov::preprocess::ColorFormat::NV12_SINGLE_PLANE);
    return tensor;
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v8::NV12toBGR>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    auto b_dim = static_cast<int>(targetShape[1] * 2 / (3 * targetShape[2]));
    if (node->inputs().size() > 1 || b_dim < 2)
        return generate(std::static_pointer_cast<ov::Node>(node), port, elemType, targetShape);
    ov::Tensor tensor(elemType, targetShape);
    color_conversion::fill_tensor(tensor, ov::preprocess::ColorFormat::NV12_SINGLE_PLANE);
    return tensor;
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v0::NormalizeL2>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 0) {
        InputGenerateData inGenData(-5, 10, 7, 222);
        return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v6::ExperimentalDetectronDetectionOutput>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    InputGenerateData inGenData(1, 0, 1, 1);
    auto tensor = ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);

    if (0 == port || 1 == port) {
#define CASE(X) case X: { \
        using T = typename ov::fundamental_type_for<X>; \
        auto raw_ptr = tensor.data<T>(); \
        if (port == 0) { \
            raw_ptr[2] = static_cast<T>(10.f); \
            raw_ptr[3] = static_cast<T>(10.f); \
        } \
        if (port == 1) { \
            raw_ptr[0] = static_cast<T>(5.f); \
        } \
        break; \
    }

        switch (elemType) {
            CASE(ov::element::Type_t::bf16)
            CASE(ov::element::Type_t::f16)
            CASE(ov::element::Type_t::f32)
            CASE(ov::element::Type_t::f64)
            default: OPENVINO_THROW("Unsupported element type: ", elemType);
        }
#undef CASE
    }
    return tensor;
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v6::ExperimentalDetectronGenerateProposalsSingleImage>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    InputGenerateData inGenData(1, 0, 1, 1);
    return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v6::ExperimentalDetectronPriorGridGenerator>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    InputGenerateData inGenData(0, 0, 1, 1);
    if (0 == port) {
        inGenData.start_from = -100;
        inGenData.range = 200;
        inGenData.resolution = 2;
    }
    return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v8::MatrixNms>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 1) {
        InputGenerateData inGenData(0, 1, 1000, 1);
        return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v5::NonMaxSuppression>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 1) {
        InputGenerateData inGenData(0, 1, 1000, 1);
        return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

ov::Tensor generate(const
                             std::shared_ptr<ov::op::v9::NonMaxSuppression>& node,
                             size_t port,
                             const ov::element::Type& elemType,
                             const ov::Shape& targetShape) {
    if (port == 1) {
        InputGenerateData inGenData(0, 1, 1000, 1);
        return ov::test::utils::create_and_fill_tensor(elemType, targetShape, inGenData);
    }
    return generate(std::dynamic_pointer_cast<ov::Node>(node), port, elemType, targetShape);
}

template<typename T>
ov::Tensor generateInput(const std::shared_ptr<ov::Node>& node,
                                  size_t port,
                                  const ov::element::Type& elemType,
                                  const ov::Shape& targetShape) {
    return generate(ov::as_type_ptr<T>(node), port, elemType, targetShape);
}
} // namespace

InputsMap getInputMap() {
    static InputsMap inputsMap{
#define _OPENVINO_OP_REG(NAME, NAMESPACE) {NAMESPACE::NAME::get_type_info_static(), generateInput<NAMESPACE::NAME>},

#include "openvino/opsets/opset1_tbl.hpp"
#include "openvino/opsets/opset2_tbl.hpp"
#include "openvino/opsets/opset3_tbl.hpp"
#include "openvino/opsets/opset4_tbl.hpp"
#include "openvino/opsets/opset5_tbl.hpp"
#include "openvino/opsets/opset6_tbl.hpp"
#include "openvino/opsets/opset7_tbl.hpp"
#include "openvino/opsets/opset8_tbl.hpp"
#include "openvino/opsets/opset9_tbl.hpp"
#include "openvino/opsets/opset10_tbl.hpp"
#include "openvino/opsets/opset11_tbl.hpp"
#include "openvino/opsets/opset12_tbl.hpp"
#include "openvino/opsets/opset13_tbl.hpp"

#include "ov_ops/opset_private_tbl.hpp"
#undef _OPENVINO_OP_REG
    };
    return inputsMap;
}

} // namespace utils
} // namespace test
} // namespace ov
