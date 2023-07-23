#pragma once

#include "utility.h"

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

class SetBoolProcessor : public Processor {
public:
  SetBoolProcessor() {}
  virtual ~SetBoolProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation();
  const bool getResult() const { return result_; }

private:
  absl::string_view source_;
  std::function<bool(absl::string_view)> matcher_ = [](absl::string_view str) -> bool { return false; };
  bool result_;
};


using SetBoolProcessorSharedPtr = std::shared_ptr<SetBoolProcessor>;
using SetBoolProcessorMapSharedPtr = std::shared_ptr<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>;

class ConditionProcessor : public Processor {
public:
  ConditionProcessor() {}
  virtual ~ConditionProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(SetBoolProcessorMapSharedPtr bool_processors);

  SetBoolProcessorMapSharedPtr getBoolProcessors() { return set_bool_processors_; }
  void setBoolProccessors(SetBoolProcessorMapSharedPtr bool_processors) { set_bool_processors_ = bool_processors; }
  bool getConditionResult() const { return condition_; }

private:
  std::vector<Utility::BooleanOperatorType> operators_;
  std::vector<std::pair<absl::string_view, bool>> operands_; // operand and whether that operand is negated
  SetBoolProcessorMapSharedPtr set_bool_processors_ = nullptr;
  bool condition_;
};

using ConditionProcessorSharedPtr = std::shared_ptr<ConditionProcessor>;

class HeaderProcessor : public Processor {
public:
  HeaderProcessor() {}
  virtual ~HeaderProcessor() {}
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors) { return absl::OkStatus(); }
  virtual absl::Status evaluateCondition();
  bool getCondition() const { return condition_; }
  void setCondition(bool result) { condition_ = result; }
  void setConditionProcessor(ConditionProcessorSharedPtr condition_processor) { condition_processor_ = condition_processor; }
  ConditionProcessorSharedPtr getConditionProcessor() { return condition_processor_; }

protected:
  ConditionProcessorSharedPtr condition_processor_ = nullptr;
  absl::Status ConditionProcessorSetup(std::vector<absl::string_view>& condition_expression, std::vector<absl::string_view>::iterator start);

private:
  bool condition_;
};

class SetHeaderProcessor : public HeaderProcessor {
public:
  SetHeaderProcessor() {}
  virtual ~SetHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual absl::Status executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors);
private:
  // Note: the values returned by these functions must not outlive the SetHeaderProcessor object
  const std::string& getKey() const { return header_key_; }
  const std::vector<std::string>& getVals() const { return header_vals_; }

  void setKey(absl::string_view key) { header_key_ = std::string(key); }
  void setVals(std::vector<std::string> vals) { header_vals_ = vals; }

  std::string header_key_; // header key to set
  std::vector<std::string> header_vals_; // header values to set
};

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
