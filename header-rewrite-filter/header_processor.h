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
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) { return absl::OkStatus(); }

protected:
  SetBoolProcessorMapSharedPtr bool_processors_;
  bool is_request_; // header rewrite filter has already verified that the operation is always either http-request or http-response
};

class DynamicFunctionProcessor : public Processor {
public:
  DynamicFunctionProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : Processor(bool_processors, isRequest) {}
  virtual ~DynamicFunctionProcessor() {}
  virtual absl::Status parseOperation(absl::string_view function_expression);
  std::tuple<absl::Status, std::string> executeOperation(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks);

private:
  std::tuple<absl::Status, std::string> getFunctionArgument(absl::string_view function_expression);
  Utility::FunctionType getFunctionType(absl::string_view function_expression);
  std::tuple<absl::Status, std::string> getUrlp(Http::RequestOrResponseHeaderMap& headers, absl::string_view param);
  std::tuple<absl::Status, std::string> getHeaderValue(Http::RequestOrResponseHeaderMap& headers, absl::string_view key, int position);

  Utility::FunctionType function_type_;
  std::string function_argument_;
};

using DynamicFunctionProcessorSharedPtr = std::shared_ptr<DynamicFunctionProcessor>;

class SetBoolProcessor : public Processor {
public:
  SetBoolProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : Processor(bool_processors, isRequest) {}
  virtual ~SetBoolProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual std::tuple<absl::Status, bool> executeOperation(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks, bool negate);  // return status and bool result

private:
  std::function<bool(std::string)> matcher_ = [](std::string str) -> bool { return false; };
  DynamicFunctionProcessorSharedPtr dynamic_function_processor_ = nullptr;
};

class ConditionProcessor : public Processor {
public:
  ConditionProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : Processor(bool_processors, isRequest) {}
  virtual ~ConditionProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual std::tuple<absl::Status, bool> executeOperation(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks); // return status and condition result

private:
  std::vector<Utility::BooleanOperatorType> operators_;
  std::vector<std::tuple<std::string, bool>> operands_; // operand and whether that operand is negated
};

using ConditionProcessorSharedPtr = std::shared_ptr<ConditionProcessor>;

class HeaderProcessor : public Processor {
public:
  HeaderProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : Processor(bool_processors, isRequest) {}
  virtual ~HeaderProcessor() {}
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks) { return absl::OkStatus(); }
  virtual std::tuple<absl::Status, bool> evaluateCondition(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks); // return status and condition result
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
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks);
private:
  // Note: the values returned by these functions must not outlive the SetHeaderProcessor object
  const absl::string_view getKey() const { return header_key_; }
  const std::string getVal() const { return header_val_; }

  void setKey(absl::string_view key) { header_key_ = std::string(key); }
  void setVal(absl::string_view val) { header_val_ = std::string(val); }

  std::string header_key_; // header key to set
  std::string header_val_; // header value to set
};

class AppendHeaderProcessor : public HeaderProcessor {
public:
  AppendHeaderProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : HeaderProcessor(bool_processors, isRequest) {}
  virtual ~AppendHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks);
private:
  // Note: the values returned by these functions must not outlive the AppendHeaderProcessor object
  const absl::string_view getKey() const { return header_key_; }
  const std::vector<std::string>& getVals() const { return header_vals_; }

  void setKey(absl::string_view key) { header_key_ = std::string(key); }
  void setVals(std::vector<std::string> vals) { header_vals_ = vals; }

  std::string header_key_; // header key to set
  std::vector<std::string> header_vals_; // header values to set
};

// Note: path being set here includes the query string
class SetPathProcessor : public HeaderProcessor {
public:
  SetPathProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : HeaderProcessor(bool_processors, isRequest) {}
  virtual ~SetPathProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks);

private:
  // Note: the values returned by these functions must not outlive the SetHeaderProcessor object
  const std::string& getPath() const { return request_path_; }
  void setPath(absl::string_view path) { request_path_ = std::string(path); }

  std::string request_path_; // path to set
};

class SetDynamicMetadataProcessor : public HeaderProcessor {
public:
  SetDynamicMetadataProcessor(SetBoolProcessorMapSharedPtr bool_processors, bool isRequest) : HeaderProcessor(bool_processors, isRequest) {}
  virtual ~SetDynamicMetadataProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, Http::StreamFilterCallbacks* callbacks);

private:
  // Note: the values returned by these functions must not outlive the SetDynamicMetadataProcessor object
  std::string metadata_key_;
  DynamicFunctionProcessorSharedPtr metadata_value_ = nullptr;
};

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
