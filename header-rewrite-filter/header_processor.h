#pragma once
#include "utility.h"

#include "source/common/common/utility.h"
#include "source/common/http/utility.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include <string>
#include <vector>

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

class SetBoolProcessor;
using SetBoolProcessorSharedPtr = std::shared_ptr<SetBoolProcessor>;
using SetBoolProcessorMapSharedPtr = std::shared_ptr<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>;

class Processor {
public:
  Processor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : bool_processors_(bool_processors), is_request_(isRequest)  { }
  virtual ~Processor() {}
  virtual absl::Status parseOperation([[maybe_unused]] std::vector<absl::string_view>& operation_expression, [[maybe_unused]] std::vector<absl::string_view>::iterator start) { return absl::OkStatus(); }

protected:
  SetBoolProcessorMapSharedPtr bool_processors_;
  const bool is_request_; // header rewrite filter has already verified that the operation is always either http-request or http-response
};

class DynamicFunctionProcessor : public Processor {
public:
  DynamicFunctionProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : Processor(bool_processors, isRequest) {}
  virtual ~DynamicFunctionProcessor() {}
  virtual absl::Status parseOperation(absl::string_view function_expression);
  std::tuple<absl::Status, std::string> executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo);

private:
  using Processor::parseOperation;
  std::tuple<absl::Status, std::string> getFunctionArgument(absl::string_view function_expression);
  Utility::FunctionType getFunctionType(absl::string_view function_expression);
  std::tuple<absl::Status, std::string> getUrlp(Http::RequestOrResponseHeaderMap& headers, absl::string_view param);
  std::tuple<absl::Status, std::string> getHeaderValue(Http::RequestOrResponseHeaderMap& headers, absl::string_view key, int position);
  std::tuple<absl::Status, std::string> getDynamicMetadata(Envoy::StreamInfo::StreamInfo* streamInfo, absl::string_view key);

  Utility::FunctionType function_type_;
  std::string function_argument_;
};

using DynamicFunctionProcessorSharedPtr = std::shared_ptr<DynamicFunctionProcessor>;

class SetBoolProcessor : public Processor {
public:
  SetBoolProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : Processor(bool_processors, isRequest) {}
  virtual ~SetBoolProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual std::tuple<absl::Status, bool> executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo, bool negate); // return status and bool result

private:
  absl::Status stringToCompareSetup(absl::string_view string_to_compare);
  std::function<bool(const std::string, const std::string)> matcher_ = []([[maybe_unused]] const std::string& source, [[maybe_unused]] const std::string& string_to_compare) -> bool { return false; };
  DynamicFunctionProcessorSharedPtr source_processor_ = nullptr;
  DynamicFunctionProcessorSharedPtr string_to_compare_function_processor_ = nullptr;
};

class ConditionProcessor : public Processor {
public:
  ConditionProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : Processor(bool_processors, isRequest) {}
  virtual ~ConditionProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual std::tuple<absl::Status, bool> executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo); // return status and condition result
  virtual std::tuple<absl::Status, bool> executeOperationRecursively(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo,
    std::vector<Utility::BooleanOperatorType>::iterator operators_start, std::vector<Utility::BooleanOperatorType>::iterator operators_end,
    std::vector<std::tuple<std::string, bool>>::iterator operands_start, std::vector<std::tuple<std::string, bool>>::iterator operands_end);
  virtual std::tuple<absl::Status, bool> executeOperationLinearly(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo, 
    std::vector<Utility::BooleanOperatorType>::iterator operators_start, std::vector<Utility::BooleanOperatorType>::iterator operators_end,
    std::vector<std::tuple<std::string, bool>>::iterator operands_start, std::vector<std::tuple<std::string, bool>>::iterator operands_end);

private:
  std::vector<Utility::BooleanOperatorType> operators_;
  std::vector<std::tuple<std::string, bool>> operands_; // operand and whether that operand is negated
};

using ConditionProcessorSharedPtr = std::shared_ptr<ConditionProcessor>;

class HeaderProcessor : public Processor {
public:
  HeaderProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : Processor(bool_processors, isRequest) {}
  virtual ~HeaderProcessor() {}
  virtual absl::Status executeOperation([[maybe_unused]] Http::RequestOrResponseHeaderMap& headers, [[maybe_unused]] Envoy::StreamInfo::StreamInfo* streamInfo) { return absl::OkStatus(); }
  virtual std::tuple<absl::Status, bool> evaluateCondition(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo); // return status and condition result
  void setConditionProcessor(ConditionProcessorSharedPtr condition_processor) { condition_processor_ = condition_processor; }
  ConditionProcessorSharedPtr getConditionProcessor() { return condition_processor_; }

protected:
  ConditionProcessorSharedPtr condition_processor_ = nullptr;
  absl::Status ConditionProcessorSetup(std::vector<absl::string_view>& condition_expression, std::vector<absl::string_view>::iterator start);
};

class SetHeaderProcessor : public HeaderProcessor {
public:
  SetHeaderProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : HeaderProcessor(bool_processors, isRequest) {}
  virtual ~SetHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo);
private:
  DynamicFunctionProcessorSharedPtr header_key_ = nullptr; // header key to set
  DynamicFunctionProcessorSharedPtr header_val_ = nullptr; // header value to set
};

class AppendHeaderProcessor : public HeaderProcessor {
public:
  AppendHeaderProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : HeaderProcessor(bool_processors, isRequest) {}
  virtual ~AppendHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo);
  
private:
  DynamicFunctionProcessorSharedPtr header_key_ = nullptr; // header key to set
  std::vector<DynamicFunctionProcessorSharedPtr> header_vals_; // header values to append
};

// Note: path being set here includes the query string
class SetPathProcessor : public HeaderProcessor {
public:
  SetPathProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : HeaderProcessor(bool_processors, isRequest) {}
  virtual ~SetPathProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo);

private:
  DynamicFunctionProcessorSharedPtr request_path_; // path to set
};

class SetDynamicMetadataProcessor : public HeaderProcessor {
public:
  SetDynamicMetadataProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : HeaderProcessor(bool_processors, isRequest) {}
  virtual ~SetDynamicMetadataProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo);

private:
  // Note: the values returned by these functions must not outlive the SetDynamicMetadataProcessor object
  DynamicFunctionProcessorSharedPtr metadata_key_ = nullptr;
  DynamicFunctionProcessorSharedPtr metadata_value_ = nullptr;
};

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
