// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <fstream>
#include <map>
#include <string>

static std::map<std::string, std::string> parseConfig(const std::string& configName, char comment = '#') {
    std::map<std::string, std::string> config = {};

    std::ifstream file(configName);
    if (!file.is_open()) {
        return config;
    }

    std::string key, value;
    while (file >> key >> value) {
        if (key.empty() || key[0] == comment) {
            continue;
        }
        config[key] = value;
    }

    return config;
}
