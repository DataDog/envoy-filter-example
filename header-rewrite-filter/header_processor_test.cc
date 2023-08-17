#include "gtest/gtest.h"
#include "header_processor.h"
#include "source/common/common/utility.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "test/integration/http_integration.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

using ::testing::ReturnRef;

class ProcessorTest : public ::testing::Test {
protected:
    void SetUp() override { }
};

using SetBoolProcessorSharedPtr = std::shared_ptr<SetBoolProcessor>;
using SetBoolProcessorMapSharedPtr = std::shared_ptr<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>;

// Note: processors assume that the first two arguments are validated (these arguments are validated by the filter)

TEST_F(ProcessorTest, SetHeaderProcessorTest) {
    Envoy::StreamInfo::MockStreamInfo* stream_info;
    std::vector<absl::string_view> positive_test_cases = {
        "http-request set-header mock_header mock_value", // can set one value
        "http-request set-header mock_header another_mock_value" // replaces already existing value
    };

    std::vector<absl::string_view> negative_test_cases = {
        "http-response set-header mock_key mock_val1 mock_val2", // can't set multiple values
        "http-request set-header mock_key", // missing argument
        "http-request set-header mock_key mock_val if" // empty condition
    };

    for (const auto operation_expression : positive_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetHeaderProcessor set_header_processor = SetHeaderProcessor(nullptr, (tokens.at(0) == "http-request"));
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_header_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status == absl::OkStatus());
        status = set_header_processor.executeOperation(headers, stream_info);
        EXPECT_TRUE(status == absl::OkStatus());
        EXPECT_EQ(tokens.at(3), headers.get(Http::LowerCaseString(tokens.at(2)))[0]->value().getStringView());
    }

    for (const auto operation_expression : negative_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetHeaderProcessor set_header_processor = SetHeaderProcessor(nullptr, true);
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_header_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, AppendHeaderProcessorTest) {
    Envoy::StreamInfo::MockStreamInfo* stream_info;
    AppendHeaderProcessor append_header_processor = AppendHeaderProcessor(nullptr, true);
    Http::TestRequestHeaderMapImpl headers{
        {":method", "GET"}, {":path", "/"}, {":authority", "host"}};

    // can set one value
    std::vector<absl::string_view> operation_expression({"http-request", "append-header", "mock_header", "mock_value"});
    absl::Status status = append_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    status = append_header_processor.executeOperation(headers, stream_info);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_EQ("mock_value", headers.get(Http::LowerCaseString("mock_header"))[0]->value().getStringView());

    // can set multiple values
    AppendHeaderProcessor append_header_processor_multiple_values = AppendHeaderProcessor(nullptr, true);
    operation_expression = {"http-request", "append-header", "mock_key", "mock_val1", "mock_val2"};
    status = append_header_processor_multiple_values.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    status = append_header_processor_multiple_values.executeOperation(headers, stream_info);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_EQ("mock_val1,mock_val2", headers.get(Http::LowerCaseString("mock_key"))[0]->value().getStringView());

    std::vector<absl::string_view> negative_test_cases = {
        "http-response append-header mock_key", // missing argument
        "http-request append-header mock_key mock_val if" // empty condition
    };

    for (const auto operation_expression : negative_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        AppendHeaderProcessor append_header_processor = AppendHeaderProcessor(nullptr, (tokens.at(0) == "http-request"));
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = append_header_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, SetPathProcessorTest) {
    Envoy::StreamInfo::MockStreamInfo* stream_info;
    std::vector<absl::string_view> positive_test_cases = {
        // correctly updates path and preserves query string
        "http-request set-path mock_path",
        "http-request set-path new_path"
    };

    std::vector<absl::string_view> negative_test_cases = {
        "http-response set-path", // missing argument
        "http-request set-path mock_path extra_arg", // extra arg
        "http-request set-path mock_path if" // empty condition
    };

    for (const auto operation_expression : positive_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetPathProcessor set_path_processor = SetPathProcessor(nullptr, (tokens.at(0) == "http-request"));
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/?param=1"}, {":authority", "host"}};
        absl::Status status = set_path_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status == absl::OkStatus());
        status = set_path_processor.executeOperation(headers, stream_info);
        EXPECT_TRUE(status == absl::OkStatus());
        std::string expected_path = std::string(tokens.at(2)) + "?param=1";
        EXPECT_EQ(expected_path, headers.get(Http::LowerCaseString(":path"))[0]->value().getStringView());
    }

    for (const auto operation_expression : negative_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetPathProcessor set_path_processor = SetPathProcessor(nullptr, (tokens.at(0) == "http-request"));
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_path_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, SetBoolProcessorTest) {
    Envoy::StreamInfo::MockStreamInfo* stream_info;

    std::vector<absl::string_view> true_match_test_cases = {
        "http-request set-bool mock_bool %[hdr(mock_header1)] -m str mock_value3", // exact, hdr 1 arg
        "http-request set-bool mock_bool %[hdr(mock_header1,-1)] -m str mock_value3", // exact, hdr 2 arg
        "http-request set-bool mock_bool %[hdr(mock_header1,0)] -m str mock_value1", // exact, hdr 2 arg
        "http-request set-bool mock_bool %[hdr(mock_header1,-3)] -m str mock_value1", // exact, hdr 2 arg
        "http-request set-bool mock_bool %[hdr(mock_header1)] -m str %[hdr(mock_header3)]", // exact, hdr 2 arg, both dynamic
        "http-request set-bool mock_bool %[hdr(mock_header3,0)] -m str []", // exact
        "http-request set-bool mock_bool %[hdr(mock_header2)] -m beg mo", // prefix
        "http-request set-bool mock_bool %[urlp(param1)] -m beg so", // prefix, urlp
        "http-request set-bool mock_bool %[hdr(mock_header2)] -m sub lue", // substring
        "http-request set-bool mock_bool %[hdr(mock_header2)] -m found", // found, hdr
        "http-request set-bool mock_bool %[urlp(param2)] -m found" // found, urlp
    };

    std::vector<absl::string_view> false_match_test_cases = {
        "http-request set-bool mock_bool %[hdr(mock_header2)] -m str no-match", // exact
        "http-request set-bool mock_bool %[hdr(mock_header1)] -m beg tch", // prefix
        "http-request set-bool mock_bool %[hdr(mock_header2)] -m sub not_a_substring", // substring
        "http-request set-bool mock_bool %[hdr(unknown_header)] -m found" // found
    };

    std::vector<absl::string_view> negative_test_cases = {
        "http-request set-bool mock_bool %[hdr(mock_header1)] -m str", // missing argument
        "http-request set-bool mock_bool %[hdr(mock_header1)] -m", // missing argument
        "http-response set-bool mock_bool %[urlp(param1)] -m beg so", // urlp on response side
        "http-request set-bool mock_bool %[urlp(param1)] -m str arg extra_arg", // extra arg
        "http-request set-bool mock_bool %[urlp(param1)] -m found extra_arg", // extra arg
        "http-request set-bool mock_bool %[urlp(param2)] str matches", // missing -m flag
        "http-request set-bool mock_bool %[urlp(param2 -m found)]" // invalid syntax
    };

    for (const auto operation_expression : true_match_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetBoolProcessor set_bool_processor = SetBoolProcessor(nullptr, (tokens.at(0) == "http-request"));
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/?param1=something&param2=2"}, {":authority", "host"}, 
            {"mock_header1", "mock_value1,mock_value2,mock_value3"}, {"mock_header2", "mock_value"},
            {"mock_header3", "[],mock_value3"}};
        absl::Status status = set_bool_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status == absl::OkStatus());
        EXPECT_EQ(status.message(), "");
        std::tuple<absl::Status, bool> result = set_bool_processor.executeOperation(headers, stream_info, false);
        status = std::get<0>(result);
        bool bool_result = std::get<1>(result);
        EXPECT_TRUE(status == absl::OkStatus());
        EXPECT_TRUE(bool_result);
    }

    for (const auto operation_expression : false_match_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetBoolProcessor set_bool_processor = SetBoolProcessor(nullptr, (tokens.at(0) == "http-request"));
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/?param1=1,param2=2"}, {":authority", "host"}, {"mock_header1", "mock_value1,mock_value2,mock_value3"}, {"mock_header2", "mock_value"}};
        absl::Status status = set_bool_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status == absl::OkStatus());
        EXPECT_EQ(status.message(), "");
        std::tuple<absl::Status, bool> result = set_bool_processor.executeOperation(headers, stream_info, false);
        status = std::get<0>(result);
        bool bool_result = std::get<1>(result);
        EXPECT_TRUE(status == absl::OkStatus());
        EXPECT_FALSE(bool_result);
    }

    for (const auto operation_expression : negative_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetBoolProcessor set_bool_processor = SetBoolProcessor(nullptr, (tokens.at(0) == "http-request"));
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/?param1=1,param2=2"}, {":authority", "host"}, {"mock_header1", "mock_value1,mock_value2,mock_value3"}, {"mock_header2", "mock_value"}};
        absl::Status status = set_bool_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, ConditionProcessorTest) {
    Envoy::StreamInfo::MockStreamInfo* stream_info;
    Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}, {"mock_header", "mock_value"}};
            
    SetBoolProcessorMapSharedPtr mock_bool_processors = std::make_shared<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>();
    SetBoolProcessorSharedPtr mock_true_bool_processor = std::make_shared<SetBoolProcessor>(mock_bool_processors, true);
    SetBoolProcessorSharedPtr mock_false_bool_processor = std::make_shared<SetBoolProcessor>(mock_bool_processors, true);

    // set up mock bool processors
    std::vector<absl::string_view> operation_expression = {"http", "set-bool", "mock_true_bool", "%[hdr(mock_header)]", "-m", "str", "mock_value"};
    absl::Status status = mock_true_bool_processor->parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    operation_expression = {"http", "set-bool", "mock_false_bool", "%[hdr(mock_header)]", "-m", "str", "not-a-match"};
    status = mock_false_bool_processor->parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    mock_bool_processors->insert({"mock_true_bool", mock_true_bool_processor});
    mock_bool_processors->insert({"mock_false_bool", mock_false_bool_processor});

    // verify bool map entries
    EXPECT_EQ(mock_bool_processors->size(), 2);
    EXPECT_NO_THROW(mock_bool_processors->at("mock_true_bool"));
    EXPECT_NO_THROW(mock_bool_processors->at("mock_false_bool"));

    std::vector<absl::string_view> true_condition_test_cases = {
        "mock_true_bool",
        "not mock_false_bool",
        "mock_true_bool and mock_true_bool",
        "mock_true_bool or mock_false_bool",
        "mock_true_bool or not mock_false_bool or mock_false_bool",
        "mock_true_bool or mock_false_bool and mock_false_bool",
        "mock_true_bool or not mock_false_bool or not mock_true_bool and mock_true_bool or mock_false_bool and mock_false_bool or mock_true_bool"
    };

    std::vector<absl::string_view> false_condition_test_cases = {
        "mock_false_bool",
        "not mock_true_bool",
        "mock_false_bool and not mock_true_bool or not mock_true_bool or mock_true_bool and mock_false_bool or mock_false_bool or not mock_true_bool",
        "not mock_true_bool or mock_true_bool and not mock_true_bool",
        "mock_true_bool and mock_true_bool and not mock_false_bool and mock_false_bool or mock_true_bool and mock_false_bool and mock_true_bool or not mock_true_bool"
    };

    std::vector<absl::string_view> invalid_parsing_test_cases = {
        "and mock_true_bool",
        "mock_true_bool and or mock_true_bool",
        "mock_true_bool not and mock_true_bool",
        "mock_true_bool and mock_true_bool not",
        "non_existent_bool"
    };

    for (const auto operation_expression : true_condition_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        ConditionProcessor condition_processor = ConditionProcessor(mock_bool_processors, true);
        absl::Status status = condition_processor.parseOperation(tokens, tokens.begin());
        EXPECT_TRUE(status == absl::OkStatus());
        std::tuple<absl::Status, bool> result = condition_processor.executeOperation(headers, stream_info);
        status = std::get<0>(result);
        EXPECT_TRUE(status == absl::OkStatus());
        bool bool_result = std::get<1>(result);
        EXPECT_TRUE(bool_result);
    }

    for (const auto operation_expression : false_condition_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        ConditionProcessor condition_processor = ConditionProcessor(mock_bool_processors, true);
        absl::Status status = condition_processor.parseOperation(tokens, tokens.begin());
        EXPECT_TRUE(status == absl::OkStatus());
        std::tuple<absl::Status, bool> result = condition_processor.executeOperation(headers, stream_info);
        status = std::get<0>(result);
        EXPECT_TRUE(status == absl::OkStatus());
        bool bool_result = std::get<1>(result);
        EXPECT_TRUE(!bool_result);
    }

    for (const auto operation_expression : invalid_parsing_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        ConditionProcessor condition_processor = ConditionProcessor(mock_bool_processors, true);
        absl::Status status = condition_processor.parseOperation(tokens, tokens.begin());
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, DynamicMetadataTest) {
    std::vector<std::tuple<absl::string_view, absl::string_view>> positive_test_cases = {
        std::make_tuple("http-request set-metadata mock_key %[hdr(mock_header,-1)]", "http-request set-header metadata_value %[metadata(mock_key)]"),
        std::make_tuple("http-request set-metadata another_mock_key %[urlp(param1)]", "http-request set-header another_mock_header %[metadata(another_mock_key)]")
    };

    std::vector<absl::string_view> negative_parsing_test_cases = {
        "http-request set-metadata mock_key", // missing argument
        "http-request set-metadata mock_key %[hdr(mock_header) extra_arg" // extra argument
        "http-request set-metadata mock_key %[hdr() extra_arg" // missing dynamic function argument
    };

    std::vector<absl::string_view> negative_execution_test_cases = {
        "http-request set-metadata mock_key %[hdr(non_existent_header)]", // nonexistent header
    };

    Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/?param1=hello"}, {":authority", "host"}, {"mock_header", "mock_value"}};
    
    // create mock and set up mock call
    NiceMock<StreamInfo::MockStreamInfo> stream_info;
    envoy::config::core::v3::Metadata dynamic_metadata;
    google::protobuf::Struct filter_metadata = ProtobufWkt::Struct::default_instance();
    (*dynamic_metadata.mutable_filter_metadata())["envoy.extensions.filters.http.HeaderRewrite"] = filter_metadata;
    ON_CALL(stream_info, dynamicMetadata()).WillByDefault(ReturnRef(dynamic_metadata));
    
    for (const auto operation_expression : positive_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(std::get<0>(operation_expression), " ", false, true);
        SetDynamicMetadataProcessor dynamic_metadata_processor = SetDynamicMetadataProcessor(nullptr, tokens.at(0) == "http-request");
        absl::Status status = dynamic_metadata_processor.parseOperation(tokens, tokens.begin() + 2);
        EXPECT_TRUE(status == absl::OkStatus());
        status = dynamic_metadata_processor.executeOperation(headers, &stream_info);
        EXPECT_TRUE(status == absl::OkStatus());

        // confirm that metadata can be fetched
        std::vector<absl::string_view> verify_tokens = StringUtil::splitToken(std::get<1>(operation_expression), " ", false, true);
        SetHeaderProcessor set_header_processor = SetHeaderProcessor(nullptr, verify_tokens.at(0) == "http-request");
        status = set_header_processor.parseOperation(verify_tokens, verify_tokens.begin() + 2);
        EXPECT_TRUE(status == absl::OkStatus());
        status = set_header_processor.executeOperation(headers, &stream_info);
        EXPECT_TRUE(status == absl::OkStatus());
    }

    for (const auto operation_expression : negative_parsing_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetDynamicMetadataProcessor dynamic_metadata_processor = SetDynamicMetadataProcessor(nullptr, tokens.at(0) == "http-request");
        absl::Status status = dynamic_metadata_processor.parseOperation(tokens, tokens.begin() + 2);
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }

    for (const auto operation_expression : negative_execution_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetDynamicMetadataProcessor dynamic_metadata_processor = SetDynamicMetadataProcessor(nullptr, tokens.at(0) == "http-request");
        absl::Status status = dynamic_metadata_processor.parseOperation(tokens, tokens.begin() + 2);
        EXPECT_TRUE(status == absl::OkStatus());
        status = dynamic_metadata_processor.executeOperation(headers, &stream_info);
        EXPECT_TRUE(status != absl::OkStatus());
    }
}

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy