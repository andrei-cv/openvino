// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <set>
#include <string>

namespace ov {
namespace tools {
namespace subgraph_dumper {

struct ModelInfo {
    std::set<std::string> model_paths;
    size_t this_op_cnt, total_op_cnt, model_priority;

    ModelInfo(const std::string& model_path = "", size_t total_ops_in_model = 1, size_t _model_priority = 1) :
        total_op_cnt(total_ops_in_model), model_paths({model_path}),
        this_op_cnt(1), model_priority(_model_priority) {}

    bool operator==(const ModelInfo& model_info_ref) const {
        if (this->model_priority != model_info_ref.model_priority || this->this_op_cnt != model_info_ref.this_op_cnt ||
            this->total_op_cnt != model_info_ref.total_op_cnt || this->model_paths.size() != model_info_ref.model_paths.size()) {
            return false;
        }
        for (const auto& model_path : this->model_paths) {
            if (std::find(model_info_ref.model_paths.begin(), model_info_ref.model_paths.end(), model_path) == model_info_ref.model_paths.end()) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace subgraph_dumper
}  // namespace tools
}  // namespace ov
