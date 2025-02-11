# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

from mo.ops.op import Op


class ExperimentalDetectronPriorGridGenerator(Op):
    op = 'ExperimentalDetectronPriorGridGenerator'

    def __init__(self, graph, attrs):
        mandatory_props = dict(
            type=self.op,
            op=self.op,
            version='opset6',
            infer=self.infer,
        )
        super().__init__(graph, mandatory_props, attrs)

    def backend_attrs(self):
        return [
            'flatten',
            'h',
            'w',
            'stride_x',
            'stride_y',
        ]

    @staticmethod
    def infer(node):
        input_shape = node.in_port(0).data.get_shape()
        priors_num = input_shape[0]
        grid_h = node.in_port(1).data.get_shape()[2]
        grid_w = node.in_port(1).data.get_shape()[3]
        if node.flatten:
            out_shape = [grid_h * grid_w * priors_num, 4]
        else:
            out_shape = [grid_h, grid_w, priors_num, 4]
        node.out_port(0).data.set_shape(out_shape)
