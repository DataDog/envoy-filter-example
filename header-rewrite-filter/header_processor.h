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
  virtual ~HeaderProcessor() {};
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression) = 0;
  virtual void executeOperation(Http::RequestHeaderMap& headers) const = 0;
  virtual absl::Status evaluateCondition() = 0;
  bool getCondition() const { return condition_; }
  void setCondition(bool result) { condition_ = result; }

protected:
  bool condition_;
};

class SetHeaderProcessor : public HeaderProcessor {
public:
  SetHeaderProcessor();
  virtual ~SetHeaderProcessor() {}
  virtual absl::Status parseOperation(std::vector<absl::string_view>& operation_expression);
  virtual void executeOperation(Http::RequestHeaderMap& headers) const;
  virtual absl::Status evaluateCondition(); // TODO: will need to pass http-related metadata in order to evaluate dynamic values

  // Note: the values returned by these functions must not outlive the SetHeaderProcessor object
  const std::string& getKey() const { return header_key_; }
  const std::vector<std::string>& getVals() const { return header_vals_; }

  void setKey(absl::string_view key) { header_key_ = std::string(key); }
  void setVals(std::vector<std::string> vals) { header_vals_ = vals; }

  // TODO: should each operation store an error?
  void setError() { error_ = true; }
  bool getError() { return error_; }

private:
  std::string header_key_; // header key to set
  std::vector<std::string> header_vals_; // header values to set
  bool error_ = false;
};

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
