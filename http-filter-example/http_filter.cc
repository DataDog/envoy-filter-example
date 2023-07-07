#include <string>
#include <unordered_map>
#include <sstream>
#include <vector>

#include "http_filter.h"

#include "source/common/common/utility.h"
#include "source/common/common/logger.h"
#include "envoy/server/filter_config.h"

namespace Envoy {
namespace Http {

void fail(absl::string_view msg) {
  auto& logger = Logger::Registry::getLog(Logger::Id::tracing);
  ENVOY_LOG_TO_LOGGER(logger, error, "Failed to parse config - {}", msg);
}

HttpSampleDecoderFilterConfig::HttpSampleDecoderFilterConfig(
    const sample::Decoder& proto_config)
    : key_(proto_config.key()), val_(proto_config.val()), extra_(stoi(proto_config.extra())) {}

HttpSampleDecoderFilter::HttpSampleDecoderFilter(HttpSampleDecoderFilterConfigSharedPtr config)
    : config_(config) {
  // populate list of valid operations, implementation will be changed in the next few commits
  operations_.insert("set-header");
  
  const std::string header_config = headerValue();

  // find the position of the first space character
  const size_t spacePos = header_config.find(' ');
  if (spacePos == std::string::npos) {
    fail("no arguments provided");
    setError(1);
    return;
  }

  // can't be a string view
  const auto operation = header_config.substr(0, spacePos);

  // error checking -- invalid operation
  if (operations_.find(operation) == operations_.end()) {
    fail("invalid operation provided");
    setError(1);
    return;
  }

  const std::string args = header_config.substr(spacePos + 1);

  // create a string stream to iterate over the arguments
  std::istringstream iss(args);
  std::vector<std::string> values;
  std::string value;

  // split the rest of the string by space and store the values in a vector
  while (iss >> value) {
      values.push_back(value);
  }

  // error checking -- wrong number of arguments
  if (operation == "set-header" && values.size() != 2) {
    fail("expected 2 arguments");
    setError(1);
    return;
  }

  // insert the key-value pair into header_ops
  header_ops_[operation] = values;
}

HttpSampleDecoderFilter::~HttpSampleDecoderFilter() {}

void HttpSampleDecoderFilter::onDestroy() {}

const LowerCaseString HttpSampleDecoderFilter::headerKey() const {
  return LowerCaseString(config_->key());
}

const std::string HttpSampleDecoderFilter::headerValue() const {
  return config_->val();
}

int HttpSampleDecoderFilter::headerExtra() const {
  return config_->extra();
}

int HttpSampleDecoderFilter::getError() const {
  return error_;
}

void HttpSampleDecoderFilter::setError(const int val) {
  error_ = val;
}

FilterHeadersStatus HttpSampleDecoderFilter::decodeHeaders(RequestHeaderMap& headers, bool) {
    if (getError()) {
      fail("skipping http filter");
      return FilterHeadersStatus::Continue;
    }

  auto header_ops = header_ops_;

  // perform header operations
  for (auto const& header_op : header_ops) {
        const std::string op = header_op.first;
        if (op == "set-header") {
          headers.setCopy(LowerCaseString(header_ops[op].at(0)), header_ops[op].at(1));
        } else {
          headers.addCopy(LowerCaseString("no"), "op");
        }
  }

  return FilterHeadersStatus::Continue;
}

FilterDataStatus HttpSampleDecoderFilter::decodeData(Buffer::Instance&, bool) {
  return FilterDataStatus::Continue;
}

void HttpSampleDecoderFilter::setDecoderFilterCallbacks(StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace Http
} // namespace Envoy
