// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include "transformations/swap_input_matmul_gna.hpp"

#include "common_test_utils/ngraph_test_utils.hpp"
#include <ngraph/function.hpp>
#include <ngraph/opsets/opset8.hpp>
#include <ngraph/pass/manager.hpp>
#include <transformations/init_node_info.hpp>

namespace testing {

static std::shared_ptr<ngraph::Function> CreateMatMulFunction(const ngraph::Shape& input1_shape,
                                                              const ngraph::Shape& input2_shape,
                                                              const ngraph::Shape& bias_shape,
                                                              bool withBias,
                                                              bool withWeightsFq,
                                                              bool withOutFq,
                                                              bool swappedInputs,
                                                              bool needTranspose) {
    auto input_params = std::make_shared<ngraph::opset8::Parameter>(ngraph::element::i64, input2_shape);

    auto constant = ngraph::opset8::Constant::create(ngraph::element::i64, input1_shape, {1});
    std::shared_ptr<ngraph::Node> const_input = constant;
    if (withWeightsFq) {
        auto input_low = ngraph::opset8::Constant::create(ngraph::element::f32, ngraph::Shape{1}, {1});
        auto input_high = ngraph::opset8::Constant::create(ngraph::element::f32, ngraph::Shape{1}, {20});
        auto output_low = ngraph::opset8::Constant::create(ngraph::element::f32, ngraph::Shape{1}, {0});
        auto output_high = ngraph::opset8::Constant::create(ngraph::element::f32, ngraph::Shape{1}, {10});
        const_input = std::make_shared<ngraph::opset8::FakeQuantize>(const_input, input_low, input_high,
                                                                     output_low, output_high, 11);
    }
    auto matmul = swappedInputs ? std::make_shared<ngraph::opset8::MatMul>(input_params, const_input, needTranspose, needTranspose) :
        std::make_shared<ngraph::opset8::MatMul>(const_input, input_params, needTranspose, needTranspose);

    std::shared_ptr<ngraph::Node> final_node = matmul;
    if (withBias) {
        auto bias = ngraph::opset8::Constant::create(ngraph::element::i64, bias_shape, {1});
        std::shared_ptr<ngraph::Node> bias_node = bias;
        if (needTranspose && bias_shape.size() > 1) {
            auto transpose_order = ngraph::opset8::Constant::create(ngraph::element::i64, ngraph::Shape{2},
                                                                    std::vector<size_t>{1, 0});
            bias_node = std::make_shared<ngraph::opset8::Transpose>(bias_node, transpose_order);
        }
        final_node  = std::make_shared<ngraph::opset8::Add>(matmul, bias_node);
    }

    if (withOutFq) {
        auto input_low = ngraph::opset8::Constant::create(ngraph::element::f32, ngraph::Shape{1}, {1});
        auto input_high = ngraph::opset8::Constant::create(ngraph::element::f32, ngraph::Shape{1}, {20});
        auto output_low = ngraph::opset8::Constant::create(ngraph::element::f32, ngraph::Shape{1}, {0});
        auto output_high = ngraph::opset8::Constant::create(ngraph::element::f32, ngraph::Shape{1}, {10});
        final_node = std::make_shared<ngraph::opset8::FakeQuantize>(final_node, input_low, input_high,
                                                                    output_low, output_high, 11);
    }

    if (needTranspose) {
        auto transpose_order = ngraph::opset8::Constant::create(ngraph::element::i64, ngraph::Shape{2},
                                                                std::vector<size_t>{1, 0});
        final_node = std::make_shared<ngraph::opset8::Transpose>(final_node, transpose_order);
    }

    auto result = std::make_shared<ngraph::opset8::Result>(final_node);
    return std::make_shared<ngraph::Function>(ngraph::ResultVector{result},
                                              ngraph::ParameterVector{input_params});
}

static void Execute(std::shared_ptr<ngraph::Function> function, std::shared_ptr<ngraph::Function> reference_function) {
    ngraph::pass::Manager m;
    m.register_pass<ngraph::pass::InitNodeInfo>();
    m.register_pass<GNAPluginNS::SwapInputMatMulWithFq>();
    m.register_pass<GNAPluginNS::SwapInputMatMulWithBias>();
    m.register_pass<GNAPluginNS::SwapInputMatMul>();
    m.run_passes(function);
    ASSERT_NO_THROW(check_rt_info(function));

    const FunctionsComparator func_comparator = FunctionsComparator::with_default().enable(FunctionsComparator::ATTRIBUTES);
    const FunctionsComparator::Result result = func_comparator(function, reference_function);
    ASSERT_TRUE(result.valid);
}

typedef std::tuple<
        std::vector<ngraph::Shape>,         // constant input shape, non-const input shape, bias shape
        bool,                               // with bias
        bool,                               // with weights FakeQuantize
        bool                                // with output FakeQuantize
> SwapInputMatmulParams;

static std::string getTestCaseName(testing::TestParamInfo<SwapInputMatmulParams> obj) {
    std::vector<ngraph::Shape> shapes;
    bool withBias, withWeightsFq, withOutFq;
    std::tie(shapes, withBias, withWeightsFq, withOutFq) = obj.param;

    std::ostringstream result;
    result << "IS1=" << shapes[0] << "_";
    result << "IS2=" << shapes[1] << "_";
    result << "BS=" << shapes[2] << "_";
    result << "bias=" << withBias << "_";
    result << "wFQ=" << withWeightsFq << "_";
    result << "oFQ=" << withOutFq;
    return result.str();
}

enum class MatmulInputType {
    FirstInputConstant,
    SecondInputConstant
}; // enum class MatmulInputType

template<MatmulInputType E>
class SwapInputMatmul : public CommonTestUtils::TestsCommon,
                        public ::testing::WithParamInterface<SwapInputMatmulParams> {
public:
    void SetUp() override {
        std::vector<ngraph::Shape> shapes;
        bool withBias, withWeightsFq, withOutFq;
        std::tie(shapes, withBias, withWeightsFq, withOutFq) = this->GetParam();

        bool swap_inputs = false;
        switch (E) {
        case MatmulInputType::FirstInputConstant:
            break;
        case MatmulInputType::SecondInputConstant:
            swap_inputs = true;
            break;
        }

        function = CreateMatMulFunction(shapes[0], shapes[1], shapes[2], withBias, withWeightsFq, withOutFq, swap_inputs, false);
        reference_function = CreateMatMulFunction(shapes[0], shapes[1], shapes[2], withBias, withWeightsFq,
                                                  withOutFq, !swap_inputs, true);
    }
public:
    std::shared_ptr<ngraph::Function> function, reference_function;
};

template<MatmulInputType E>
class SwapInputMatmulNotApplied : public CommonTestUtils::TestsCommon,
                                  public ::testing::WithParamInterface<SwapInputMatmulParams> {
public:
    void SetUp() override {
        std::vector<ngraph::Shape> shapes;
        bool withBias, withWeightsFq, withOutFq;
        std::tie(shapes, withBias, withWeightsFq, withOutFq) = this->GetParam();

        bool swap_inputs = false;
        switch (E) {
        case MatmulInputType::FirstInputConstant:
            break;
        case MatmulInputType::SecondInputConstant:
            swap_inputs = true;
            break;
        }

        function = CreateMatMulFunction(shapes[0], shapes[1], shapes[2], withBias, withWeightsFq, withOutFq, swap_inputs, false);
        reference_function = ngraph::clone_function(*function);
    }
public:
    std::shared_ptr<ngraph::Function> function, reference_function;
};

using SwapInputMatmulWithFirstInputConstant = SwapInputMatmul<MatmulInputType::FirstInputConstant>;
using SwapInputMatmulWithSecondInputConstant = SwapInputMatmul<MatmulInputType::SecondInputConstant>;
using SwapInputMatmulWithFirstInputConstantNotApplied = SwapInputMatmulNotApplied<MatmulInputType::FirstInputConstant>;
using SwapInputMatmulWithSecondInputConstantNotApplied = SwapInputMatmulNotApplied<MatmulInputType::SecondInputConstant>;

TEST_P(SwapInputMatmulWithFirstInputConstant, CompareFunctions) {
    Execute(function, reference_function);
}

TEST_P(SwapInputMatmulWithFirstInputConstantNotApplied, CompareFunctions) {
    Execute(function, reference_function);
}

TEST_P(SwapInputMatmulWithSecondInputConstant, CompareFunctions) {
    Execute(function, reference_function);
}

TEST_P(SwapInputMatmulWithSecondInputConstantNotApplied, CompareFunctions) {
    Execute(function, reference_function);
}

const std::vector<std::vector<ngraph::Shape>> input_shapes_for_matmul_with_first_constant_applied = {
    {{16, 8}, {8, 8}, {16, 8}},
    {{16, 8}, {8, 8}, {1}},
};

const std::vector<std::vector<ngraph::Shape>> input_shapes_for_matmul_with_first_constant_not_applied = {
    {{1, 8}, {8, 8}, {1, 8}},
    {{8}, {8, 8}, {8}}
};

const std::vector<std::vector<ngraph::Shape>> input_shapes_for_matmul_with_second_constant_applied = {
    {{64, 6}, {100, 64}, {100, 6}},
    {{64, 6}, {100, 64}, {1}},
};

const std::vector<std::vector<ngraph::Shape>> input_shapes_for_matmul_with_second_constant_not_applied = {
    {{64, 16}, {100, 64}, {100, 16}},
    {{64, 6}, {8, 64}, {8, 6}},
    {{8, 1}, {8, 8}, {8, 1}},
    {{8}, {8, 8}, {8}}
};

INSTANTIATE_TEST_SUITE_P(smoke_swap_input_matmul, SwapInputMatmulWithFirstInputConstant,
    ::testing::Combine(
        ::testing::ValuesIn(input_shapes_for_matmul_with_first_constant_applied),
        ::testing::ValuesIn(std::vector<bool>{false, true}),
        ::testing::ValuesIn(std::vector<bool>{false, true}),
        ::testing::ValuesIn(std::vector<bool>{false, true})),
    getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_swap_input_matmul, SwapInputMatmulWithFirstInputConstantNotApplied,
    ::testing::Combine(
        ::testing::ValuesIn(input_shapes_for_matmul_with_first_constant_not_applied),
        ::testing::ValuesIn(std::vector<bool>{false, true}),
        ::testing::ValuesIn(std::vector<bool>{false, true}),
        ::testing::ValuesIn(std::vector<bool>{false, true})),
    getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_swap_input_matmul, SwapInputMatmulWithSecondInputConstant,
    ::testing::Combine(
        ::testing::ValuesIn(input_shapes_for_matmul_with_second_constant_applied),
        ::testing::ValuesIn(std::vector<bool>{false, true}),
        ::testing::ValuesIn(std::vector<bool>{false, true}),
        ::testing::ValuesIn(std::vector<bool>{false, true})),
    getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_swap_input_matmul, SwapInputMatmulWithSecondInputConstantNotApplied,
    ::testing::Combine(
        ::testing::ValuesIn(input_shapes_for_matmul_with_second_constant_not_applied),
        ::testing::ValuesIn(std::vector<bool>{false, true}),
        ::testing::ValuesIn(std::vector<bool>{false, true}),
        ::testing::ValuesIn(std::vector<bool>{false, true})),
    getTestCaseName);

} // namespace testing
