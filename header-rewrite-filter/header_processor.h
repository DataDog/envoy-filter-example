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

class Processor {
public:
  Processor() {}
  virtual ~Processor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) { return absl::OkStatus(); }
};

class DynamicFunctionProcessor : public Processor {
public:
  DynamicFunctionProcessor() {}
  virtual ~DynamicFunctionProcessor() {}
  virtual absl::Status parseOperation(absl::string_view function_expression, const bool isRequest);
  std::tuple<absl::Status, std::string> executeOperation(Http::RequestOrResponseHeaderMap& headers);

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
  SetBoolProcessor() {}
  virtual ~SetBoolProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual std::tuple<absl::Status, bool> executeOperation(Http::RequestOrResponseHeaderMap& headers, bool negate);  // return status and bool result

private:
  std::function<bool(std::string)> matcher_ = [](std::string str) -> bool { return false; };
  DynamicFunctionProcessorSharedPtr dynamic_function_processor_ = nullptr;
};


using SetBoolProcessorSharedPtr = std::shared_ptr<SetBoolProcessor>;
using SetBoolProcessorMapSharedPtr = std::shared_ptr<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>;

class ConditionProcessor : public Processor {
public:
  ConditionProcessor() {}
  virtual ~ConditionProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual std::tuple<absl::Status, bool> executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors); // return status and condition result

private:
  std::vector<Utility::BooleanOperatorType> operators_;
  std::vector<std::tuple<std::string, bool>> operands_; // operand and whether that operand is negated
};

using ConditionProcessorSharedPtr = std::shared_ptr<ConditionProcessor>;

class HeaderProcessor : public Processor {
public:
  HeaderProcessor() {}
  virtual ~HeaderProcessor() {}
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors) { return absl::OkStatus(); }
  virtual std::tuple<absl::Status, bool> evaluateCondition(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors); // return status and condition result
  void setConditionProcessor(ConditionProcessorSharedPtr condition_processor) { condition_processor_ = condition_processor; }
  ConditionProcessorSharedPtr getConditionProcessor() { return condition_processor_; }

protected:
  ConditionProcessorSharedPtr condition_processor_ = nullptr;
  absl::Status ConditionProcessorSetup(std::vector<absl::string_view>& condition_expression, std::vector<absl::string_view>::iterator start);
};

class SetHeaderProcessor : public HeaderProcessor {
public:
  SetHeaderProcessor() {}
  virtual ~SetHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors);
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
  AppendHeaderProcessor() {}
  virtual ~AppendHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors);
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
  SetPathProcessor() {}
  virtual ~SetPathProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors);

private:
  const std::string& getPath() const { return request_path_; }
  void setPath(absl::string_view path) { request_path_ = std::string(path); }

  std::string request_path_; // path to set
};

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
