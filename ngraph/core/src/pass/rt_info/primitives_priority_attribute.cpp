// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "transformations/rt_info/primitives_priority_attribute.hpp"

#include <assert.h>

#include <functional>
#include <iterator>
#include <memory>
#include <ngraph/node.hpp>
#include <ngraph/opsets/opset1.hpp>
#include <ngraph/variant.hpp>
#include <ostream>

using namespace ov;
using namespace ngraph;

std::string ov::getPrimitivesPriority(const std::shared_ptr<ngraph::Node>& node) {
    const auto& rtInfo = node->get_rt_info();

    if (!rtInfo.count(PrimitivesPriority::get_type_info_static()))
        return "";

    const auto& attr = rtInfo.at(PrimitivesPriority::get_type_info_static());
    return ov::as_type_ptr<PrimitivesPriority>(attr)->get();
}

std::shared_ptr<ngraph::Variant> PrimitivesPriority::merge(const ngraph::NodeVector& nodes) {
    auto isConvolutionBased = [](const std::shared_ptr<Node>& node) -> bool {
        if (std::dynamic_pointer_cast<ngraph::opset1::Convolution>(node) ||
            std::dynamic_pointer_cast<ngraph::opset1::GroupConvolution>(node) ||
            std::dynamic_pointer_cast<ngraph::opset1::GroupConvolutionBackpropData>(node) ||
            std::dynamic_pointer_cast<ngraph::opset1::ConvolutionBackpropData>(node)) {
            return true;
        }
        return false;
    };

    std::set<std::string> unique_pp;

    for (auto& node : nodes) {
        if (isConvolutionBased(node)) {
            std::string pp = getPrimitivesPriority(node);
            if (!pp.empty())
                unique_pp.insert(pp);
        }
    }

    if (unique_pp.size() > 1) {
        throw ngraph_error(std::string(get_type_info()) + " no rule defined for multiple values.");
    }

    std::string final_primitives_priority;
    if (unique_pp.size() == 1) {
        final_primitives_priority = *unique_pp.begin();
    }
    return std::make_shared<PrimitivesPriority>(final_primitives_priority);
}

std::shared_ptr<ngraph::Variant> PrimitivesPriority::init(const std::shared_ptr<ngraph::Node>& node) {
    throw ngraph_error(std::string(get_type_info()) + " has no default initialization.");
}

bool PrimitivesPriority::visit_attributes(AttributeVisitor& visitor) {
    visitor.on_attribute("value", m_value);
    return true;
}
