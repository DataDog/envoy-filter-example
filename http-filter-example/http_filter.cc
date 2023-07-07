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
  
  const std::string header_config = headerValue();

  // split by operation (comma delimited config)
  auto operations = StringUtil::splitToken(header_config, "\n", false, true);

  // process each operation
  for (auto const& operation : operations) {
    auto tokens = StringUtil::splitToken(operation, " ");
    if (tokens.size() < 3) {
      fail("too few arguments provided");
      setError(1); // TODO: set error based on globally defined macros
      return;
    }

    // determine if it's request/response
    if (tokens[0] != "http-request" && tokens[0] != "http-response") {
      fail("first argument must be <http-response/http-request>");
      setError(1); // TODO: set error based on globally defined macros
      return;
    }
    bool isRequest = (tokens[0] == "http-request");

    // TODO: determine the operation type (right now I'm assuming that it's always a set-header operation)

    // make new header processor
    SetHeaderProcessor* processor = new SetHeaderProcessor();

    // parse operation
    if(processor->parseOperation(tokens)) {
      fail("unable to parse operation");
      setError(1); // TODO: set error based on globally defined macros
      return;
    }

    // keep track of operations to be executed
    if (isRequest) {
      request_header_processors_.push_back(processor);
    }
  }
}

HttpSampleDecoderFilter::~HttpSampleDecoderFilter() {
  onDestroy(); // not sure if this is called automatically somewhere else
}

void HttpSampleDecoderFilter::onDestroy() {
  for (auto processor : request_header_processors_) {
    free(processor);
  }
}

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
  int err = 0;

  if (err) {
    return FilterHeadersStatus::Continue;
  }

  // execute each operation
  // TODO: run this loop for the response side too once filter type is changed to Encoder/Decoder
  for (auto const processor : request_header_processors_) {
    if(processor->executeOperation(headers)) {
      fail("unable to execute operation");
      return FilterHeadersStatus::Continue;
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
