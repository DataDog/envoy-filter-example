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

// Note: both processors assume that the first two arguments are validated (these arguments are validated by the filter)

TEST_F(ProcessorTest, SetHeaderProcessorTest) {
    SetHeaderProcessor set_header_processor = SetHeaderProcessor();
    Http::TestRequestHeaderMapImpl headers{
        {":method", "GET"}, {":path", "/"}, {":authority", "host"}};

    // TODO: will be changed once append-header operation is added
    // can set one value
    std::vector<absl::string_view> operation_expression({"http-request", "set-header", "mock_header", "mock_value"});
    absl::Status status = set_header_processor.parseOperation(operation_expression);
    EXPECT_TRUE(status == absl::OkStatus());
    set_header_processor.executeOperation(headers);
    EXPECT_EQ("mock_value", headers.get(Http::LowerCaseString("mock_header"))[0]->value().getStringView());

    // can set multiple values
    operation_expression = {"http-response", "set-header", "mock_key", "mock_val1", "mock_val2"};
    status = set_header_processor.parseOperation(operation_expression);
    EXPECT_TRUE(status == absl::OkStatus());
    set_header_processor.executeOperation(headers);
    EXPECT_EQ("mock_val1", headers.get(Http::LowerCaseString("mock_key"))[0]->value().getStringView());
    EXPECT_EQ("mock_val2", headers.get(Http::LowerCaseString("mock_key"))[1]->value().getStringView());

    // missing argument
    operation_expression = {"http-response", "set-header", "mock_key"};
    status = set_header_processor.parseOperation(operation_expression);
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "not enough arguments for set-header");

}

TEST_F(ProcessorTest, SetPathProcessorTest) {
    SetPathProcessor set_path_processor = SetPathProcessor();
    Http::TestRequestHeaderMapImpl headers{
        {":method", "GET"}, {":path", "/"}, {":authority", "host"}};

    std::vector<absl::string_view> operation_expression({"http-request", "set-path", "mock_path"});
    absl::Status status = set_path_processor.parseOperation(operation_expression);
    EXPECT_TRUE(status == absl::OkStatus());
    set_path_processor.executeOperation(headers);
    EXPECT_EQ("mock_path", headers.get(Http::LowerCaseString(":path"))[0]->value().getStringView());

    // missing argument
    operation_expression = {"http-request", "set-path"};
    status = set_path_processor.parseOperation(operation_expression);
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "not enough arguments for set-path");
}

TEST_F(ProcessorTest, SetBoolProcessorTest) {
    SetBoolProcessor set_bool_processor = SetBoolProcessor();

    Http::TestRequestHeaderMapImpl headers{
        {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
    
    // positive exact match
    std::vector<absl::string_view> operation_expression({"http", "set-bool", "mock_bool", "matches", "-m", "str", "matches"});
    absl::Status status = set_bool_processor.parseOperation(operation_expression);
    EXPECT_TRUE(status == absl::OkStatus());
    set_bool_processor.executeOperation(headers);
    EXPECT_TRUE(set_bool_processor.getResult());

    // negative exact match
    operation_expression = {"http", "set-bool", "mock_bool", "no-match", "-m", "str", "match"};
    status = set_bool_processor.parseOperation(operation_expression);
    EXPECT_TRUE(status == absl::OkStatus());
    set_bool_processor.executeOperation(headers);
    EXPECT_FALSE(set_bool_processor.getResult());

    // invalid number of arguments
    operation_expression = {"http", "set-bool", "mock_bool", "matches", "-m", "str"};
    status = set_bool_processor.parseOperation(operation_expression);
    EXPECT_EQ(status.message(), "error parsing boolean expression");

    // missing -m flag
    operation_expression = {"http", "set-bool", "mock_bool", "matches", "str", "matches"};
    status = set_bool_processor.parseOperation(operation_expression);
    EXPECT_EQ(status.message(), "invalid match syntax");
    EXPECT_FALSE(set_bool_processor.getResult());
}

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy