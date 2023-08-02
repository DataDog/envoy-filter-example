#include "gtest/gtest.h"
#include "header_processor.h"
#include "source/common/common/utility.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "test/integration/http_integration.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

using ::testing::Return;

class ProcessorTest : public ::testing::Test {
protected:
    void SetUp() override { }
};

class MockSetBoolProcessor : public SetBoolProcessor {
public:
    MOCK_METHOD(absl::Status, parseOperation, (std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start));
    MOCK_METHOD((std::tuple<absl::Status, bool>), executeOperation, (bool negate));
};
using MockSetBoolProcessorSharedPtr = std::shared_ptr<MockSetBoolProcessor>;
using MockSetBoolProcessorMapSharedPtr = std::shared_ptr<std::unordered_map<std::string, MockSetBoolProcessorSharedPtr>>;
using SetBoolProcessorSharedPtr = std::shared_ptr<SetBoolProcessor>;
using SetBoolProcessorMapSharedPtr = std::shared_ptr<std::unordered_map<std::string, MockSetBoolProcessorSharedPtr>>;

// Note: processors assume that the first two arguments are validated (these arguments are validated by the filter)

TEST_F(ProcessorTest, SetHeaderProcessorTest) {
    SetHeaderProcessor set_header_processor = SetHeaderProcessor();
    Http::TestRequestHeaderMapImpl headers{
        {":method", "GET"}, {":path", "/"}, {":authority", "host"}};

    // can set one value
    std::vector<absl::string_view> operation_expression({"http-request", "set-header", "mock_header", "mock_value"});
    absl::Status status = set_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    status = set_header_processor.executeOperation(headers, nullptr);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_EQ("mock_value", headers.get(Http::LowerCaseString("mock_header"))[0]->value().getStringView());

    // replaces already existing value
    operation_expression = {"http-request", "set-header", "mock_header", "another_mock_value"};
    status = set_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    status = set_header_processor.executeOperation(headers, nullptr);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_EQ("another_mock_value", headers.get(Http::LowerCaseString("mock_header"))[0]->value().getStringView());

    // can't set multiple values
    operation_expression = {"http-response", "set-header", "mock_key", "mock_val1", "mock_val2"};
    status = set_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "third argument to set-header must be a condition");

    // missing argument
    operation_expression = {"http-response", "set-header", "mock_key"};
    status = set_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "not enough arguments for set-header");

    // empty condition
    operation_expression = {"http-response", "set-header", "mock_key", "mock_val", "if"};
    status = set_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "empty condition provided");

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

    // missing argument
    operation_expression = {"http-response", "append-header", "mock_key"};
    status = append_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "not enough arguments for append-header");

    // empty condition
    operation_expression = {"http-response", "append-header", "mock_key", "mock_val1", "mock_val2", "if"};
    status = append_header_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "empty condition provided");
}

TEST_F(ProcessorTest, SetPathProcessorTest) {
    SetPathProcessor set_path_processor = SetPathProcessor();
    Http::TestRequestHeaderMapImpl headers{
        {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
    
    // confirm current path
    EXPECT_EQ("/", headers.get(Http::LowerCaseString(":path"))[0]->value().getStringView());

    // correctly updates path
    std::vector<absl::string_view> operation_expression({"http-request", "set-path", "mock_path?param=1"});
    absl::Status status = set_path_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    status = set_path_processor.executeOperation(headers, nullptr);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_EQ("mock_path?param=1", headers.get(Http::LowerCaseString(":path"))[0]->value().getStringView());

    // preserves query string
    operation_expression = {"http-request", "set-path", "new_path"};
    status = set_path_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    status = set_path_processor.executeOperation(headers, nullptr);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_EQ("new_path?param=1", headers.get(Http::LowerCaseString(":path"))[0]->value().getStringView());

    // missing argument
    operation_expression = {"http-request", "set-path"};
    status = set_path_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "not enough arguments for set-path");

    // empty condition
    operation_expression = {"http-response", "set-path", "mock_path", "if"};
    status = set_path_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "empty condition provided");
}

TEST_F(ProcessorTest, SetBoolProcessorTest) {
    SetBoolProcessor set_bool_processor = SetBoolProcessor();

    std::vector<absl::string_view> operation_expression;
    std::tuple<absl::Status, bool> result;
    absl::Status status;
    bool bool_result;

    Http::TestRequestHeaderMapImpl headers{
        {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
    
    // positive exact match
    operation_expression = {"http", "set-bool", "mock_bool", "matches", "-m", "str", "matches"};
    status = set_bool_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    result = set_bool_processor.executeOperation(false);
    status = std::get<0>(result);
    bool_result = std::get<1>(result);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_TRUE(bool_result);

    // negative exact match
    operation_expression = {"http", "set-bool", "mock_bool", "no-match", "-m", "str", "match"};
    status = set_bool_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status == absl::OkStatus());
    result = set_bool_processor.executeOperation(false);
    status = std::get<0>(result);
    bool_result = std::get<1>(result);
    EXPECT_TRUE(status == absl::OkStatus());
    EXPECT_FALSE(bool_result);

    // too few arguments for exact match
    operation_expression = {"http", "set-bool", "mock_bool", "matches", "-m", "str"};
    status = set_bool_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "missing argument for match type");

    // too many arguments for exact match
    operation_expression = {"http", "set-bool", "mock_bool", "matches", "-m", "str", "arg", "extra_arg"};
    status = set_bool_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "too many arguments for set bool");

    // missing -m flag
    operation_expression = {"http", "set-bool", "mock_bool", "matches", "str", "matches"};
    status = set_bool_processor.parseOperation(operation_expression, (operation_expression.begin() + 2));
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "invalid match syntax");
}

TEST_F(ProcessorTest, ConditionProcessorTest) {
    ConditionProcessor condition_processor = ConditionProcessor();
    MockSetBoolProcessorMapSharedPtr mock_bool_processors = std::make_shared<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>();
    MockSetBoolProcessorSharedPtr mock_true_bool_processor = std::make_shared<MockSetBoolProcessor>();
    MockSetBoolProcessorSharedPtr mock_false_bool_processor = std::make_shared<MockSetBoolProcessor>();

    std::vector<absl::string_view> operation_expression;
    std::tuple<absl::Status, bool> result;
    absl::Status status;
    bool bool_result;

    // set up mock bool processors
    ON_CALL(*mock_true_bool_processor, parseOperation(_, _)).WillByDefault(Return(absl::OkStatus()));
    ON_CALL(*mock_true_bool_processor, executeOperation(_)).WillByDefault(Return(std::make_tuple(absl::OkStatus(), true)));
    ON_CALL(*mock_false_bool_processor, parseOperation(_, _)).WillByDefault(Return(absl::OkStatus()));
    ON_CALL(*mock_true_bool_processor, executeOperation(_)).WillByDefault(Return(std::make_tuple(absl::OkStatus(), false)));

    mock_bool_processors->insert({"mock_true_bool", mock_true_bool_processor});
    mock_bool_processors->insert({"mock_false_bool", mock_false_bool_processor});

    // empty condition provided
    operation_expression = {};
    status = condition_processor.parseOperation(operation_expression, operation_expression.begin());
    EXPECT_TRUE(status != absl::OkStatus());
    EXPECT_EQ(status.message(), "empty condition provided to condition processor");

    // single true condition
    operation_expression = {"mock_true_bool"};
    status = condition_processor.parseOperation(operation_expression, operation_expression.begin());
    EXPECT_TRUE(status == absl::OkStatus());
    result = condition_processor.executeOperation(mock_bool_processors);
    status = std::get<0>(result);
    EXPECT_TRUE(status == absl::OkStatus());
    bool_result = std::get<1>(result);
    EXPECT_TRUE(bool_result);
}

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy