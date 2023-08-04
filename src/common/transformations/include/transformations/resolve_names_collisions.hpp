// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/pass/pass.hpp"
#include "transformations_visibility.hpp"

namespace ov {
namespace pass {

/**
 * @ingroup ie_transformation_common_api
 * @brief ResolveNameCollisions transformation helps to fix names collisions
 * if nodes with autogenerated names have conflicts with other node names.
 *
 * Every transformation call can change the graph structure and create some additional operations,
 * autogenerated name is used if new operation doesn't have friendly name.
 * This transformations should be called after the transformation pipeline in order to fix names collisions.
 *
 *  There is also an additional mode "resolve_all_names", the logic of which is the same,
 * but for all "friendly_names" in the model ov, not only for autogenerated.
 */
class TRANSFORMATIONS_API ResolveNameCollisions : public ModelPass {
public:
    OPENVINO_RTTI("ResolveNameCollisions", "0");
    ResolveNameCollisions() = default;
    explicit ResolveNameCollisions(bool resolve_all_names) : m_resolve_all_names(resolve_all_names) {}
    bool run_on_model(const std::shared_ptr<ov::Model>& model) override;

private:
    bool m_resolve_all_names = false;
};

}  // namespace pass
}  // namespace ov
