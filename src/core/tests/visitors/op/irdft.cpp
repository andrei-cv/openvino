// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "common_test_utils/visitor.hpp"
#include "gtest/gtest.h"
#include "ngraph/ngraph.hpp"
#include "ngraph/op/util/attr_types.hpp"

using namespace std;
using namespace ngraph;
using ngraph::test::NodeBuilder;

TEST(attributes, irdft_op) {
    NodeBuilder::get_ops().register_factory<op::v9::IRDFT>();
    auto data = make_shared<op::v0::Parameter>(element::f32, Shape{2, 10, 10, 2});
    auto axes = op::v0::Constant::create<int64_t>(element::i64, Shape{1}, {2});
    auto irdft = make_shared<op::v9::IRDFT>(data, axes);

    NodeBuilder builder(irdft, {data, axes});
    EXPECT_NO_THROW(auto g_irdft = ov::as_type_ptr<op::v9::IRDFT>(builder.create()));

    const auto expected_attr_count = 0;
    EXPECT_EQ(builder.get_value_map_size(), expected_attr_count);
}

TEST(attributes, irdft_op_signal) {
    NodeBuilder::get_ops().register_factory<op::v9::IRDFT>();
    auto data = make_shared<op::v0::Parameter>(element::f32, Shape{2, 10, 10, 2});
    auto signal = op::Constant::create<int64_t>(element::Type_t::i64, Shape{1}, {20});
    auto axes = op::v0::Constant::create<int64_t>(element::i64, Shape{1}, {2});
    auto irdft = make_shared<op::v9::IRDFT>(data, axes, signal);

    NodeBuilder builder(irdft, {data, axes, signal});
    EXPECT_NO_THROW(auto g_irdft = ov::as_type_ptr<op::v9::IRDFT>(builder.create()));

    const auto expected_attr_count = 0;
    EXPECT_EQ(builder.get_value_map_size(), expected_attr_count);
}
