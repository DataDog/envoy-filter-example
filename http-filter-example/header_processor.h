#pragma once

#include "source/extensions/filters/http/common/pass_through_filter.h"

#include <string>
#include <vector>

namespace Envoy {
namespace Http {

class HeaderProcessor {
public:
  virtual ~HeaderProcessor() {};
  virtual int parseOperation(std::vector<absl::string_view>& operation_expression) = 0;
  virtual int executeOperation(RequestHeaderMap& headers) const = 0;
  virtual int evaluateCondition() = 0;
  bool getCondition() const { return condition_; }
  void setCondition(bool result) { condition_ = result; }

protected:
  bool condition_;
};

class SetHeaderProcessor : public HeaderProcessor {
public:
  SetHeaderProcessor();
  virtual ~SetHeaderProcessor() {}
  virtual int parseOperation(std::vector<absl::string_view>& operation_expression);
  virtual int executeOperation(RequestHeaderMap& headers) const;
  virtual int evaluateCondition(); // TODO: will need to pass http-related metadata in order to evaluate dynamic values

  const std::string& getKey() const { return header_key_; }
  const std::vector<std::string>& getVals() const { return header_vals_; }
  void setKey(std::string key) { header_key_ = key; }
  void setVals(std::vector<std::string> vals) { header_vals_ = vals; }

private:
  std::string header_key_; // header key to set
  std::vector<std::string> header_vals_; // header values to set
};

} // namespace Http
} // namespace Envoy
