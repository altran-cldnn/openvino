// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <memory>

#include "default_opset.hpp"
#include "ngraph/type/element_type.hpp"
#include "op/cast.hpp"
#include "utils/common.hpp"

namespace ngraph {
namespace onnx_import {
namespace op {
namespace set_1 {

OutputVector cast_like(const Node& node) {
    auto inputs = node.get_ng_inputs();
    return {std::make_shared<default_opset::ConvertLike>(inputs.at(0), inputs.at(1))};
}

}  // namespace set_1
}  // namespace op
}  // namespace onnx_import
}  // namespace ngraph
