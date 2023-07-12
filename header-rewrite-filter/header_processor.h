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
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression) { return absl::OkStatus(); }
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers) { }
};

class HeaderProcessor : public Processor {
public:
  virtual ~HeaderProcessor() {}
  virtual absl::Status evaluateCondition() { return absl::OkStatus(); }
  bool getCondition() const { return condition_; }
  void setCondition(bool result) { condition_ = result; }

protected:
  bool condition_;
};

class SetHeaderProcessor : public HeaderProcessor {
public:
  SetHeaderProcessor() {}
  virtual ~SetHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression);
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers);
  virtual absl::Status evaluateCondition(); // TODO: will need to pass http-related metadata in order to evaluate dynamic values

private:
  // Note: the values returned by these functions must not outlive the SetHeaderProcessor object
  const std::string& getKey() const { return header_key_; }
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
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression);
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers);
  virtual absl::Status evaluateCondition(); // TODO: possibly combine with SetHeader implementation and move code into HeaderProcessor

private:
  const std::string& getPath() const { return request_path_; }
  void setPath(absl::string_view path) { request_path_ = std::string(path); }

  std::string request_path_; // path to set
};

class SetBoolProcessor : public Processor {
public:
  virtual ~SetBoolProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression);
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers);
  const std::string& getBoolName() const { return bool_name_; }
  const bool getResult() const { return result_; }

private:
  void setBoolName(absl::string_view bool_name) { bool_name_ = std::string(bool_name); }

  const std::pair<std::string, std::string>& getStringsToCompare() const { return strings_to_compare_; }
  void setStringsToCompare(std::pair<absl::string_view, absl::string_view> strings_to_compare);

  Utility::MatchType getMatchType() const { return match_type_; }
  void setMatchType(Utility::MatchType match_type) { match_type_ = match_type; }

  std::string bool_name_; // TODO: might only need to store this in the map
  std::pair<std::string, std::string> strings_to_compare_;
  Utility::MatchType match_type_;
  bool result_;
};

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
