#include "gtest/gtest.h"
#include "header_processor.h"
#include "source/common/common/utility.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "test/integration/http_integration.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

class ProcessorTest : public ::testing::Test {
protected:
    void SetUp() override { }
};

using SetBoolProcessorSharedPtr = std::shared_ptr<SetBoolProcessor>;
using SetBoolProcessorMapSharedPtr = std::shared_ptr<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>;

// Note: processors assume that the first two arguments are validated (these arguments are validated by the filter)

TEST_F(ProcessorTest, SetHeaderProcessorTest) {
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
        SetHeaderProcessor set_header_processor = SetHeaderProcessor();
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_header_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status == absl::OkStatus());
        status = set_header_processor.executeOperation(headers, nullptr);
        EXPECT_TRUE(status == absl::OkStatus());
        EXPECT_EQ(tokens.at(3), headers.get(Http::LowerCaseString(tokens.at(2)))[0]->value().getStringView());
    }

    for (const auto operation_expression : negative_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetHeaderProcessor set_header_processor = SetHeaderProcessor();
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_header_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, AppendHeaderProcessorTest) {
    AppendHeaderProcessor append_header_processor = AppendHeaderProcessor();
    Http::TestRequestHeaderMapImpl headers{
        {":method", "GET"}, {":path", "/"}, {":authority", "host"}};

    // can set one value
    std::vector<absl::string_view> operation_expression({"http-request", "append-header", "mock_header", "mock_value"});
    absl::Status status = append_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    status = append_header_processor.executeOperation(headers, nullptr);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_EQ("mock_value", headers.get(Http::LowerCaseString("mock_header"))[0]->value().getStringView());

    // can set multiple values
    operation_expression = {"http-response", "append-header", "mock_key", "mock_val1", "mock_val2"};
    status = append_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    status = append_header_processor.executeOperation(headers, nullptr);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_EQ("mock_val1,mock_val2", headers.get(Http::LowerCaseString("mock_key"))[0]->value().getStringView());

    std::vector<absl::string_view> negative_test_cases = {
        "http-response append-header mock_key", // missing argument
        "http-request append-header mock_key mock_val if" // empty condition
    };

    for (const auto operation_expression : negative_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        AppendHeaderProcessor append_header_processor = AppendHeaderProcessor();
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = append_header_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, SetPathProcessorTest) {
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
        SetPathProcessor set_path_processor = SetPathProcessor();
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/?param=1"}, {":authority", "host"}};
        absl::Status status = set_path_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status == absl::OkStatus());
        status = set_path_processor.executeOperation(headers, nullptr);
        EXPECT_TRUE(status == absl::OkStatus());
        std::string expected_path = std::string(tokens.at(2)) + "?param=1";
        EXPECT_EQ(expected_path, headers.get(Http::LowerCaseString(":path"))[0]->value().getStringView());
    }

    for (const auto operation_expression : negative_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetPathProcessor set_path_processor = SetPathProcessor();
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_path_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, SetBoolProcessorTest) {
    std::vector<absl::string_view> true_match_test_cases = {
        "http set-bool mock_bool matches -m str matches"
    };

    std::vector<absl::string_view> false_match_test_cases = {
        "http set-bool mock_bool matches -m str no-match"
    };

    std::vector<absl::string_view> negative_test_cases = {
        "http set-bool mock_bool matches -m str", // missing argument
        "http set-bool mock_bool matches -m str arg extra_arg", // extra arg
        "http set-bool mock_bool matches str matches", // missing -m flag
    };

    for (const auto operation_expression : true_match_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetBoolProcessor set_bool_processor = SetBoolProcessor();
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_bool_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status == absl::OkStatus());
        std::tuple<absl::Status, bool> result = set_bool_processor.executeOperation(false);
        status = std::get<0>(result);
        bool bool_result = std::get<1>(result);
        EXPECT_TRUE(status == absl::OkStatus());
        EXPECT_TRUE(bool_result);
    }

    for (const auto operation_expression : false_match_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetBoolProcessor set_bool_processor = SetBoolProcessor();
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_bool_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status == absl::OkStatus());
        std::tuple<absl::Status, bool> result = set_bool_processor.executeOperation(false);
        status = std::get<0>(result);
        bool bool_result = std::get<1>(result);
        EXPECT_TRUE(status == absl::OkStatus());
        EXPECT_FALSE(bool_result);
    }

    for (const auto operation_expression : negative_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        SetBoolProcessor set_bool_processor = SetBoolProcessor();
        Http::TestRequestHeaderMapImpl headers{
            {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
        absl::Status status = set_bool_processor.parseOperation(tokens, (tokens.begin() + 2));
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

TEST_F(ProcessorTest, ConditionProcessorTest) {
    SetBoolProcessorMapSharedPtr mock_bool_processors = std::make_shared<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>();
    SetBoolProcessorSharedPtr mock_true_bool_processor = std::make_shared<SetBoolProcessor>();
    SetBoolProcessorSharedPtr mock_false_bool_processor = std::make_shared<SetBoolProcessor>();

    // set up mock bool processors
    std::vector<absl::string_view> operation_expression = {"http", "set-bool", "mock_true_bool", "exact_match", "-m", "str", "exact_match"};
    absl::Status status = mock_true_bool_processor->parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    operation_expression = {"http", "set-bool", "mock_false_bool", "no_match", "-m", "str", "not-a-match"};
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
        "mock_true_bool or not mock_false_bool or mock_false_bool"
    };

    std::vector<absl::string_view> false_condition_test_cases = {
        "mock_false_bool",
        "not mock_true_bool",
        "mock_true_bool or mock_false_bool and mock_false_bool"
    };

    std::vector<absl::string_view> invalid_parsing_test_cases = {
        "and mock_true_bool",
        "mock_true_bool and or mock_true_bool",
        "mock_true_bool not and mock_true_bool",
        "mock_true_bool and mock_true_bool not"
    };

    std::vector<absl::string_view> invalid_execution_test_cases = {
        "non_existent_bool"
    };

    for (const auto operation_expression : true_condition_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        ConditionProcessor condition_processor = ConditionProcessor();
        absl::Status status = condition_processor.parseOperation(tokens, tokens.begin());
        EXPECT_TRUE(status == absl::OkStatus());
        std::tuple<absl::Status, bool> result = condition_processor.executeOperation(mock_bool_processors);
        status = std::get<0>(result);
        EXPECT_TRUE(status == absl::OkStatus());
        bool bool_result = std::get<1>(result);
        EXPECT_TRUE(bool_result);
    }

    for (const auto operation_expression : false_condition_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        ConditionProcessor condition_processor = ConditionProcessor();
        absl::Status status = condition_processor.parseOperation(tokens, tokens.begin());
        EXPECT_TRUE(status == absl::OkStatus());
        std::tuple<absl::Status, bool> result = condition_processor.executeOperation(mock_bool_processors);
        status = std::get<0>(result);
        EXPECT_TRUE(status == absl::OkStatus());
        bool bool_result = std::get<1>(result);
        EXPECT_TRUE(!bool_result);
    }

    for (const auto operation_expression : invalid_parsing_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        ConditionProcessor condition_processor = ConditionProcessor();
        absl::Status status = condition_processor.parseOperation(tokens, tokens.begin());
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }

    for (const auto operation_expression : invalid_execution_test_cases) {
        std::vector<absl::string_view> tokens = StringUtil::splitToken(operation_expression, " ", false, true);
        ConditionProcessor condition_processor = ConditionProcessor();
        absl::Status status = condition_processor.parseOperation(tokens, tokens.begin());
        EXPECT_TRUE(status == absl::OkStatus());
        std::tuple<absl::Status, bool> result = condition_processor.executeOperation(mock_bool_processors);
        status = std::get<0>(result);
        EXPECT_TRUE(status.code() == absl::StatusCode::kInvalidArgument);
    }
}

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy