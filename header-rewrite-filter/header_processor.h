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
  virtual ~Processor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) = 0;
};

class ConditionProcessor : public Processor {
public:
  virtual ~ConditionProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  bool executeOperation();

private:
  std::vector<Utility::BooleanOperatorType> operators_;
  std::vector<std::pair<absl::string_view, bool>> operands_; // operand and whether that operand is negated
  bool isTrue_; // Note: will be used when connecting ConditionProcessor to SetBoolProcessor
};

using ConditionProcessorSharedPtr = std::shared_ptr<ConditionProcessor>;

class HeaderProcessor : public Processor {
public:
  virtual ~HeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) = 0;
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers) = 0;
  virtual void evaluateCondition() = 0; // TODO: implement this in parent class
  bool getCondition() const { return condition_; }
  void setCondition(bool result) { condition_ = result; }
  void setConditionProcessor(ConditionProcessorSharedPtr condition_processor) { condition_processor_ = condition_processor; }
  ConditionProcessorSharedPtr getConditionProcessor() { return condition_processor_; }

protected:
  bool condition_;
  ConditionProcessorSharedPtr condition_processor_ = nullptr;
};

class SetHeaderProcessor : public HeaderProcessor {
public:
  SetHeaderProcessor() {}
  virtual ~SetHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers);
  virtual void evaluateCondition(); // TODO: will need to pass http-related metadata in order to evaluate dynamic values

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
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers);
  virtual void evaluateCondition(); // TODO: possibly combine with SetHeader implementation and move code into HeaderProcessor

private:
  const std::string& getPath() const { return request_path_; }
  void setPath(absl::string_view path) { request_path_ = std::string(path); }

  std::string request_path_; // path to set
};

class SetBoolProcessor : public Processor {
public:
  virtual ~SetBoolProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start);
  virtual bool executeOperation() const;
  const std::string& getBoolName() const { return bool_name_; }

private:
  void setBoolName(absl::string_view bool_name) { bool_name_ = std::string(bool_name); }

  const std::pair<std::string, std::string>& getStringsToCompare() const { return strings_to_compare_; }
  void setStringsToCompare(std::pair<absl::string_view, absl::string_view> strings_to_compare);

  Utility::MatchType getMatchType() const { return match_type_; }
  void setMatchType(Utility::MatchType match_type) { match_type_ = match_type; }

  std::string bool_name_;
  std::pair<std::string, std::string> strings_to_compare_;
  Utility::MatchType match_type_;
};

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
