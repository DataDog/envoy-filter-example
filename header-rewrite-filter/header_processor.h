#pragma once

#include "source/extensions/filters/http/common/pass_through_filter.h"

#include <string>
#include <vector>

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

class HeaderProcessor {
public:
  virtual ~HeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression) = 0;
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers) const = 0;
  virtual absl::Status evaluateCondition() = 0;
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
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers) const;
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

class SetPathProcessor : public HeaderProcessor {
public:
  SetPathProcessor() {}
  virtual ~SetPathProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression);
  virtual void executeOperation(Http::RequestOrResponseHeaderMap& headers) const;
  virtual absl::Status evaluateCondition(); // TODO: possibly combine with SetHeader implementation and move code into HeaderProcessor

private:
  const std::string& getPath() const { return request_path_; }
  void setPath(absl::string_view path) { request_path_ = std::string(path); }

  std::string request_path_; // path to set
};

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
