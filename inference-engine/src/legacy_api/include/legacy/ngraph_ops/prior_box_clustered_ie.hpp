// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>

#include <ie_api.h>

#include <ngraph/op/op.hpp>
#include <ngraph/op/prior_box_clustered.hpp>

namespace ngraph {
namespace op {

class INFERENCE_ENGINE_API_CLASS(PriorBoxClusteredIE) : public Op {
public:
    OPENVINO_OP("PriorBoxClusteredIE", "legacy");
    BWDCMP_RTTI_DECLARATION;

    /// \brief Constructs a PriorBoxClusteredIE operation
    ///
    /// \param layer    Layer for which prior boxes are computed
    /// \param image    Input Input to which prior boxes are scaled
    /// \param attrs          PriorBoxClustered attributes
    PriorBoxClusteredIE(const Output<Node>& input,
               const Output<Node>& image,
               const ngraph::op::PriorBoxClusteredAttrs& attrs);

    void validate_and_infer_types() override;

    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& new_args) const override;
    bool visit_attributes(AttributeVisitor& visitor) override;
    const PriorBoxClusteredAttrs& get_attrs() const { return m_attrs; }

private:
    PriorBoxClusteredAttrs m_attrs;
};

}  // namespace op
}  // namespace ngraph

