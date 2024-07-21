// Copyright (C) 2018-2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/pass/MatcherPass.hpp"
#include "transformations_visibility.hpp"

namespace ov {
namespace pass {

class TRANSFORMATIONS_API EyeDecomposition;
}  // namespace pass
}  // namespace ov

/**
 * @ingroup ov_transformation_common_api
 *
 * @brief Do eye decomposition to sub-graph (model).
 */
class ov::pass::EyeDecomposition : public MatcherPass {
public:
    OPENVINO_RTTI("EyeDecomposition", "0");
    EyeDecomposition();
};
