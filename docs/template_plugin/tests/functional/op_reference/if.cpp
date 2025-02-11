// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include <algorithm>
#include <ie_core.hpp>
#include <ie_ngraph_utils.hpp>
#include <limits>
#include <ngraph/ngraph.hpp>
#include <shared_test_classes/base/layer_test_utils.hpp>

#include "base_reference_test.hpp"

using namespace reference_tests;
using namespace ngraph;
using namespace InferenceEngine;

struct IfFunctionalBase {
    virtual std::shared_ptr<Function> create_function(const std::vector<Tensor>& if_inputs,
                                                      const std::vector<Tensor>& results) = 0;
    IfFunctionalBase() {}
};

struct IfCondConst : public IfFunctionalBase {
    std::shared_ptr<Function> create_function(const std::vector<Tensor>& if_inputs,
                                              const std::vector<Tensor>& results) override {
        NGRAPH_CHECK(if_inputs.size() == 2, "Incorrect test case! Number of inputs is not 2.");
        NGRAPH_CHECK(results.size() == 1, "Incorrect test case! Number of outputs is not 1.");

        auto X = std::make_shared<op::Parameter>(if_inputs[0].type, if_inputs[0].shape);
        auto Y = std::make_shared<op::Parameter>(if_inputs[1].type, if_inputs[1].shape);
        auto cond = std::make_shared<op::Constant>(ngraph::element::boolean, Shape{1}, cond_value);
        auto Xt = std::make_shared<op::Parameter>(if_inputs[0].type, PartialShape::dynamic());
        auto Yt = std::make_shared<op::Parameter>(if_inputs[1].type, PartialShape::dynamic());
        auto Xe = std::make_shared<op::Parameter>(if_inputs[0].type, PartialShape::dynamic());
        auto then_op = std::make_shared<op::v1::Multiply>(Xt, Yt);
        auto res0 = std::make_shared<op::Result>(then_op);
        auto res1 = std::make_shared<op::Result>(Xe);
        auto then_body = std::make_shared<ngraph::Function>(OutputVector{res0}, ParameterVector{Xt, Yt});
        auto else_body = std::make_shared<ngraph::Function>(OutputVector{res1}, ParameterVector{Xe});
        auto if_op = std::make_shared<op::v8::If>(cond);
        if_op->set_then_body(then_body);
        if_op->set_else_body(else_body);
        if_op->set_input(X, Xt, Xe);
        if_op->set_input(Y, Yt, nullptr);
        auto result = if_op->set_output(res0, res1);
        auto res = std::make_shared<op::Result>(result);
        auto fun = std::make_shared<Function>(OutputVector{res}, ParameterVector{X, Y});
        return fun;
    }

    explicit IfCondConst(bool value) : cond_value(value) {}
    bool cond_value;
};

struct IfCondIsNonConst : public IfFunctionalBase {
    std::shared_ptr<Function> create_function(const std::vector<Tensor>& if_inputs,
                                              const std::vector<Tensor>& results) override {
        NGRAPH_CHECK(if_inputs.size() == 3, "Incorrect test case! Number of inputs is not 3.");
        NGRAPH_CHECK(results.size() == 1, "Incorrect test case! Number of outputs is not 1.");

        auto X = std::make_shared<op::Parameter>(element::f32, Shape{1, 2, 2});
        auto Y = std::make_shared<op::Parameter>(element::f32, Shape{1, 2, 2});
        auto cond = std::make_shared<op::Parameter>(element::boolean, Shape{1});
        // Set up the cell body, a function from (Xi, Yi) -> (Zo)
        // Body parameters
        auto Xt = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Yt = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Xe = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Ye = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        // Body
        auto then_op = std::make_shared<op::v1::Multiply>(Xt, Yt);
        auto else_op = std::make_shared<op::v1::Add>(Xe, Ye);
        auto then_op_result = std::make_shared<op::Result>(then_op);
        auto else_op_result = std::make_shared<op::Result>(else_op);
        auto then_body = std::make_shared<ngraph::Function>(OutputVector{then_op_result}, ParameterVector{Xt, Yt});
        auto else_body = std::make_shared<ngraph::Function>(OutputVector{else_op_result}, ParameterVector{Xe, Ye});
        auto if_op = std::make_shared<op::v8::If>(cond);
        if_op->set_then_body(then_body);
        if_op->set_else_body(else_body);
        if_op->set_input(X, Xt, Xe);
        if_op->set_input(Y, Yt, Ye);
        auto result = if_op->set_output(then_op_result, else_op_result);
        auto res = std::make_shared<op::Result>(result);
        auto fun = std::make_shared<Function>(OutputVector{res}, ParameterVector{cond, X, Y});
        return fun;
    }
};

struct IfWithoutAdditionalInputs : IfFunctionalBase {
    std::shared_ptr<Function> create_function(const std::vector<Tensor>& if_inputs,
                                              const std::vector<Tensor>& results) override {
        NGRAPH_CHECK(if_inputs.size() == 1, "Incorrect test case! Number of inputs is not 1.");
        NGRAPH_CHECK(results.size() == 1, "Incorrect test case! Number of outputs is not 1.");

        auto cond = std::make_shared<op::Parameter>(element::boolean, Shape{1});
        auto A = std::make_shared<op::Constant>(element::f32, Shape{1}, 8.0);
        auto B = std::make_shared<op::Constant>(element::f32, Shape{1}, 2.0);
        auto A_res = std::make_shared<op::Result>(A);
        auto B_res = std::make_shared<op::Result>(B);
        auto then_body = std::make_shared<ngraph::Function>(OutputVector{A_res}, ParameterVector{});
        auto else_body = std::make_shared<ngraph::Function>(OutputVector{B_res}, ParameterVector{});
        auto if_op = std::make_shared<op::v8::If>(cond);
        if_op->set_then_body(then_body);
        if_op->set_else_body(else_body);
        auto res = if_op->set_output(A_res, B_res);
        auto fun = std::make_shared<Function>(OutputVector{res}, ParameterVector{cond});
        return fun;
    }
};

struct IfDynamismCaseWithStaticInputs : public IfFunctionalBase {
    std::shared_ptr<Function> create_function(const std::vector<Tensor>& if_inputs,
                                              const std::vector<Tensor>& results) override {
        NGRAPH_CHECK(if_inputs.size() == 4, "Incorrect test case! Number of inputs is not 4.");
        NGRAPH_CHECK(results.size() == 2, "Incorrect test case! Number of outputs is not 2.");

        auto X = std::make_shared<op::Parameter>(element::f32, Shape{1, 2, 2});
        auto Y = std::make_shared<op::Parameter>(element::f32, Shape{4, 2, 2});
        auto Z = std::make_shared<op::Parameter>(element::f32, Shape{8, 8, 8});
        auto cond = std::make_shared<op::Parameter>(element::boolean, Shape{1});
        // Set up the cell body, a function from (Xi, Yi) -> (Zo)
        // Body parameters
        auto Xt = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Yt = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Xe = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Ze = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        // Body
        auto then_op = std::make_shared<op::v1::Multiply>(Xt, Xt);
        auto else_op = std::make_shared<op::v1::Add>(Xe, Xe);
        auto then_op_result1 = std::make_shared<op::Result>(then_op);
        auto then_op_result2 = std::make_shared<op::Result>(Yt);
        auto else_op_result1 = std::make_shared<op::Result>(else_op);
        auto else_op_result2 = std::make_shared<op::Result>(Ze);
        auto then_body =
            std::make_shared<ngraph::Function>(OutputVector{then_op_result1, then_op_result2}, ParameterVector{Xt, Yt});
        auto else_body =
            std::make_shared<ngraph::Function>(OutputVector{else_op_result1, else_op_result2}, ParameterVector{Xe, Ze});
        auto if_op = std::make_shared<op::v8::If>(cond);
        if_op->set_then_body(then_body);
        if_op->set_else_body(else_body);
        if_op->set_input(X, Xt, Xe);
        if_op->set_input(Y, Yt, nullptr);
        if_op->set_input(Z, nullptr, Ze);
        auto res1 = if_op->set_output(then_op_result1, else_op_result1);
        auto res2 = if_op->set_output(then_op_result2, else_op_result2);
        auto result_if1 = std::make_shared<op::Result>(res1);
        auto result_if2 = std::make_shared<op::Result>(res2);
        auto fun = std::make_shared<Function>(OutputVector{result_if1, result_if2}, ParameterVector{cond, X, Y, Z});
        return fun;
    }
};

struct IfConditionIsScalar : public IfFunctionalBase {
    std::shared_ptr<Function> create_function(const std::vector<Tensor>& if_inputs,
                                              const std::vector<Tensor>& results) override {
        NGRAPH_CHECK(if_inputs.size() == 3, "Incorrect test case! Number of inputs is not 3.");
        NGRAPH_CHECK(results.size() == 1, "Incorrect test case! Number of outputs is not 1.");

        auto X = std::make_shared<op::Parameter>(element::f32, Shape{1, 2, 2});
        auto Y = std::make_shared<op::Parameter>(element::f32, Shape{1, 2, 2});
        auto cond = std::make_shared<op::Parameter>(element::boolean, Shape{});
        // Set up the cell body, a function from (Xi, Yi) -> (Zo)
        // Body parameters
        auto Xt = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Yt = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Xe = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Ye = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        // Body
        auto then_op = std::make_shared<op::v1::Multiply>(Xt, Yt);
        auto else_op = std::make_shared<op::v1::Add>(Xe, Ye);
        auto then_op_result = std::make_shared<op::Result>(then_op);
        auto else_op_result = std::make_shared<op::Result>(else_op);
        auto then_body = std::make_shared<ngraph::Function>(OutputVector{then_op_result}, ParameterVector{Xt, Yt});
        auto else_body = std::make_shared<ngraph::Function>(OutputVector{else_op_result}, ParameterVector{Xe, Ye});
        auto if_op = std::make_shared<op::v8::If>(cond);
        if_op->set_then_body(then_body);
        if_op->set_else_body(else_body);
        if_op->set_input(X, Xt, Xe);
        if_op->set_input(Y, Yt, Ye);
        auto res = if_op->set_output(then_op_result, else_op_result);
        if_op->validate_and_infer_types();
        std::vector<float> X_v{1.0, 2.0, 3.0, 4.0};
        std::vector<float> Y_v{2.0, 1.0, 2.0, 3.0};
        auto fun = std::make_shared<Function>(OutputVector{res}, ParameterVector{cond, X, Y});
        return fun;
    }
};

struct IfConditionIsDynamic : public IfFunctionalBase {
    std::shared_ptr<Function> create_function(const std::vector<Tensor>& if_inputs,
                                              const std::vector<Tensor>& results) override {
        NGRAPH_CHECK(if_inputs.size() == 3, "Incorrect test case! Number of inputs is not 3.");
        NGRAPH_CHECK(results.size() == 1, "Incorrect test case! Number of outputs is not 1.");

        auto X = std::make_shared<op::Parameter>(element::f32, Shape{1, 2, 2});
        auto Y = std::make_shared<op::Parameter>(element::f32, Shape{1, 2, 2});
        auto cond = std::make_shared<op::Parameter>(element::boolean, PartialShape{Dimension::dynamic()});
        // auto cond = std::make_shared<op::Parameter>(element::boolean, Shape{1});
        // Set up the cell body, a function from (Xi, Yi) -> (Zo)
        // Body parameters
        auto Xt = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Yt = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Xe = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        auto Ye = std::make_shared<op::Parameter>(element::f32, PartialShape::dynamic());
        // Body
        auto then_op = std::make_shared<op::v1::Multiply>(Xt, Yt);
        auto else_op = std::make_shared<op::v1::Add>(Xe, Ye);
        auto then_op_result = std::make_shared<op::Result>(then_op);
        auto else_op_result = std::make_shared<op::Result>(else_op);
        auto then_body = std::make_shared<ngraph::Function>(OutputVector{then_op_result}, ParameterVector{Xt, Yt});
        auto else_body = std::make_shared<ngraph::Function>(OutputVector{else_op_result}, ParameterVector{Xe, Ye});
        auto if_op = std::make_shared<op::v8::If>(cond);
        if_op->set_then_body(then_body);
        if_op->set_else_body(else_body);
        if_op->set_input(X, Xt, Xe);
        if_op->set_input(Y, Yt, Ye);
        auto rs = if_op->set_output(then_op_result, else_op_result);
        auto result = std::make_shared<op::Result>(rs);
        auto fun = std::make_shared<Function>(OutputVector{result}, ParameterVector{cond, X, Y});
        return fun;
    }
};

struct IfParams {
    IfParams(const std::shared_ptr<IfFunctionalBase>& functional,
             const std::vector<Tensor>& if_inputs,
             const std::vector<Tensor>& expected_results,
             const std::string& test_case_name)
        : function(functional),
          inputs(if_inputs),
          expected_results(expected_results),
          test_case_name(test_case_name) {}

    std::shared_ptr<IfFunctionalBase> function;
    std::vector<Tensor> inputs;
    std::vector<Tensor> expected_results;
    std::string test_case_name;
};

class ReferenceIfLayerTest : public testing::TestWithParam<IfParams>, public CommonReferenceTest {
public:
    void SetUp() override {
        auto params = GetParam();
        function = params.function->create_function(params.inputs, params.expected_results);
        inputData.reserve(params.inputs.size());
        refOutData.reserve(params.expected_results.size());
        for (auto& input_tensor : params.inputs) {
            inputData.push_back(input_tensor.data);
        }
        for (auto& expected_tensor : params.expected_results) {
            refOutData.push_back(expected_tensor.data);
        }
    }
    static std::string getTestCaseName(const testing::TestParamInfo<IfParams>& obj) {
        auto param = obj.param;
        return param.test_case_name;
    }
};

TEST_P(ReferenceIfLayerTest, IfWithHardcodedRefs) {
    Exec();
}
std::vector<float> Y_gen() {
    std::vector<float> Y_v;
    for (auto c_ind = 0; c_ind < 4; ++c_ind) {
        for (auto d_ind = 0; d_ind < 4; ++d_ind) {
            Y_v.push_back(static_cast<float>(c_ind * d_ind));
        }
    }
    return Y_v;
}
std::vector<float> Z_gen() {
    std::vector<float> Z_v;
    for (auto c_ind = 0; c_ind < 8; ++c_ind) {
        for (auto d_ind = 0; d_ind < 64; ++d_ind) {
            Z_v.push_back(static_cast<float>(c_ind * d_ind));
        }
    }
    return Z_v;
}

INSTANTIATE_TEST_SUITE_P(
    smoke_If_With_Hardcoded_Refs,
    ReferenceIfLayerTest,
    ::testing::Values(
        IfParams(
            std::make_shared<IfCondConst>(true),
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 1.0, 1.0, 1.0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 2.0, 2.0, 2.0})},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 2.0, 2.0, 2.0})},
            "if_condition_const_is_true"),
        IfParams(
            std::make_shared<IfCondConst>(false),
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 1.0, 1.0, 1.0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 2.0, 2.0, 2.0})},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 1.0, 1.0, 1.0})},
            "if_condition_const_is_false"),
        IfParams(
            std::make_shared<IfCondIsNonConst>(),
            std::vector<Tensor>{Tensor(Shape{1}, ngraph::element::boolean, std::vector<unsigned char>{1}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 2.0, 3.0, 4.0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 1.0, 2.0, 3.0})},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 2.0, 6.0, 12.0})},
            "if_condition_si_non_const_true"),
        IfParams(
            std::make_shared<IfCondIsNonConst>(),
            std::vector<Tensor>{Tensor(Shape{1}, ngraph::element::boolean, std::vector<unsigned char>{0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 2.0, 3.0, 4.0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 1.0, 2.0, 3.0})},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{3.0, 3.0, 5.0, 7.0})},
            "if_condition_is_non_const_false"),
        IfParams(std::make_shared<IfWithoutAdditionalInputs>(),
                 std::vector<Tensor>{Tensor(Shape{1}, ngraph::element::boolean, std::vector<unsigned char>{1})},
                 std::vector<Tensor>{Tensor(Shape{1}, ngraph::element::f32, std::vector<float>{8.0})},
                 "if_without_addition_inputs_condition_is_true"),
        IfParams(std::make_shared<IfWithoutAdditionalInputs>(),
                 std::vector<Tensor>{Tensor(Shape{1}, ngraph::element::boolean, std::vector<unsigned char>{0})},
                 std::vector<Tensor>{Tensor(Shape{1}, ngraph::element::f32, std::vector<float>{2.0})},
                 "if_without_addition_inputs_condition_is_false"),
        IfParams(
            std::make_shared<IfConditionIsScalar>(),
            std::vector<Tensor>{Tensor(Shape{}, ngraph::element::boolean, std::vector<unsigned char>{1}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 2.0, 3.0, 4.0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 1.0, 2.0, 3.0})},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 2.0, 6.0, 12.0})},
            "if_condition_is_scalar_cond_true"),
        IfParams(
            std::make_shared<IfConditionIsScalar>(),
            std::vector<Tensor>{Tensor(Shape{}, ngraph::element::boolean, std::vector<unsigned char>{0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 2.0, 3.0, 4.0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 1.0, 2.0, 3.0})},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{3.0, 3.0, 5.0, 7.0})},
            "if_condition_is_scalar_cond_false"),
        IfParams(
            std::make_shared<IfDynamismCaseWithStaticInputs>(),
            std::vector<Tensor>{Tensor(Shape{}, ngraph::element::boolean, std::vector<unsigned char>{1}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 2.0, 3.0, 4.0}),
                                Tensor(Shape{4, 2, 2}, ngraph::element::f32, Y_gen()),
                                Tensor(Shape{8, 8, 8}, ngraph::element::f32, Z_gen())},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 4.0, 9.0, 16.0}),
                                Tensor(Shape{4, 2, 2}, ngraph::element::f32, Y_gen())},
            "If_dynamism_case_with_static_inputs_condition_true"),
        IfParams(
            std::make_shared<IfDynamismCaseWithStaticInputs>(),
            std::vector<Tensor>{Tensor(Shape{}, ngraph::element::boolean, std::vector<unsigned char>{0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 2.0, 3.0, 4.0}),
                                Tensor(Shape{4, 2, 2}, ngraph::element::f32, Y_gen()),
                                Tensor(Shape{8, 8, 8}, ngraph::element::f32, Z_gen())},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 4.0, 6.0, 8.0}),
                                Tensor(Shape{8, 8, 8}, ngraph::element::f32, Z_gen())},
            "If_dynamism_case_with_static_inputs_condition_false"),
        IfParams(
            std::make_shared<IfConditionIsDynamic>(),
            std::vector<Tensor>{Tensor(Shape{}, ngraph::element::boolean, std::vector<unsigned char>{1}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 2.0, 3.0, 4.0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 1.0, 2.0, 3.0})},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 2.0, 6.0, 12.0})},
            "if_condition_is_dynamic_cond_true"),
        IfParams(
            std::make_shared<IfConditionIsDynamic>(),
            std::vector<Tensor>{Tensor(Shape{}, ngraph::element::boolean, std::vector<unsigned char>{0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{1.0, 2.0, 3.0, 4.0}),
                                Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{2.0, 1.0, 2.0, 3.0})},
            std::vector<Tensor>{Tensor(Shape{1, 2, 2}, ngraph::element::f32, std::vector<float>{3.0, 3.0, 5.0, 7.0})},
            "if_condition_is_dynamic_cond_false")));