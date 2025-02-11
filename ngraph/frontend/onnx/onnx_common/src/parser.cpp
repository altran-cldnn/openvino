// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "onnx_common/parser.hpp"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <onnx/onnx_pb.h>

#include <ngraph/file_util.hpp>

#include "ngraph/except.hpp"

namespace ngraph {
namespace onnx_common {
ONNX_NAMESPACE::ModelProto parse_from_file(const std::string& file_path) {
    std::ifstream file_stream{file_path, std::ios::in | std::ios::binary};

    if (!file_stream.is_open()) {
        throw ngraph_error("Could not open the file: " + file_path);
    };

    auto model_proto = parse_from_istream(file_stream);
    file_stream.close();
    return model_proto;
}

#if defined(OPENVINO_ENABLE_UNICODE_PATH_SUPPORT) && defined(_WIN32)
ONNX_NAMESPACE::ModelProto parse_from_file(const std::wstring& file_path) {
    std::ifstream file_stream{file_path, std::ios::in | std::ios::binary};

    if (!file_stream.is_open()) {
        NGRAPH_SUPPRESS_DEPRECATED_START
        throw ngraph_error("Could not open the file: " + file_util::wstring_to_string(file_path));
        NGRAPH_SUPPRESS_DEPRECATED_END
    };

    auto model_proto = parse_from_istream(file_stream);
    file_stream.close();
    return model_proto;
}
#endif

ONNX_NAMESPACE::ModelProto parse_from_istream(std::istream& model_stream) {
    if (!model_stream.good()) {
        model_stream.clear();
        model_stream.seekg(0);
        if (!model_stream.good()) {
            throw ngraph_error("Provided input stream has incorrect state.");
        }
    }

    ONNX_NAMESPACE::ModelProto model_proto;
    if (!model_proto.ParseFromIstream(&model_stream)) {
#ifdef NGRAPH_USE_PROTOBUF_LITE
        throw ngraph_error("Error during import of ONNX model provided as input stream "
                           " with binary protobuf message.");
#else
        // Rewind to the beginning and clear stream state.
        model_stream.clear();
        model_stream.seekg(0);
        google::protobuf::io::IstreamInputStream iistream(&model_stream);
        // Try parsing input as a prototxt message
        if (!google::protobuf::TextFormat::Parse(&iistream, &model_proto)) {
            throw ngraph_error("Error during import of ONNX model provided as input stream with prototxt "
                               "protobuf message.");
        }
#endif
    }

    return model_proto;
}
}  // namespace onnx_common
}  // namespace ngraph
