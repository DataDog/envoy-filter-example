#include <string>
#include <unordered_map>
#include <sstream>
#include <vector>

#include "http_filter.h"
#include "utility.h"

#include "source/common/common/utility.h"
#include "source/common/common/logger.h"
#include "envoy/server/filter_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SampleFilter {

void fail(absl::string_view msg) {
  auto& logger = Logger::Registry::getLog(Logger::Id::tracing);
  ENVOY_LOG_TO_LOGGER(logger, error, "Failed to parse config - {}", msg);
}

HttpSampleDecoderFilterConfig::HttpSampleDecoderFilterConfig(
    const sample::Decoder& proto_config)
    : key_(proto_config.key()), val_(proto_config.val()) {}

HttpSampleDecoderFilter::HttpSampleDecoderFilter(HttpSampleDecoderFilterConfigSharedPtr config)
    : config_(config) {
  
  const std::string header_config = headerValue();

  // split by operation (newline delimited config)
  auto operations = StringUtil::splitToken(header_config, "\n", false, true);

  // process each operation
  for (auto const& operation : operations) {
    auto tokens = StringUtil::splitToken(operation, " ");
    if (tokens.size() < Utility::MIN_NUM_ARGUMENTS) {
      fail("too few arguments provided");
      setError(1); // TODO: set error based on globally defined macros
      return;
    }

    // determine if it's request/response
    if (tokens.at(0) != Utility::HTTP_REQUEST && tokens.at(0) != Utility::HTTP_RESPONSE) {
      fail("first argument must be <http-response/http-request>");
      setError(1); // TODO: set error based on globally defined macros
      return;
    }
    const bool isRequest = (tokens.at(0) == "http-request");

    Utility::OperationType operation_type = Utility::StringToOperationType(absl::string_view(tokens.at(1)));
    HeaderProcessorUniquePtr processor;

    switch(operation_type) {
      case Utility::OperationType::kSetHeader:
        // make new header processor
        processor = std::make_unique<SetHeaderProcessor>();
        break;
      case Utility::OperationType::kSetPath:
        // TODO: implement set-path operation
        ENVOY_LOG_MISC(info, "set path operation detected!");
        return;
      default:
        fail("invalid operation type");
        setError(1); // TODO: change this to setError() once error checks PR is merged
        return;
    }

    // parse operation
    if (processor->parseOperation(tokens)) {
      fail("unable to parse operation");
      setError(1); // TODO: set error based on globally defined macros
      return;
    }

    // keep track of operations to be executed
    if (isRequest) {
      request_header_processors_.push_back(std::move(processor));
    }
  }
}

const Http::LowerCaseString HttpSampleDecoderFilter::headerKey() const {
  return Http::LowerCaseString(config_->key());
}

const std::string HttpSampleDecoderFilter::headerValue() const {
  return config_->val();
}

int HttpSampleDecoderFilter::getError() const {
  return error_;
}

void HttpSampleDecoderFilter::setError(const int val) {
  error_ = val;
}

Http::FilterHeadersStatus HttpSampleDecoderFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  int err = 0;

  if (err) {
    return Http::FilterHeadersStatus::Continue;
  }

  // execute each operation
  // TODO: run this loop for the response side too once filter type is changed to Encoder/Decoder
  for (auto const& processor : request_header_processors_) {
    if (processor->executeOperation(headers)) {
      fail("unable to execute operation");
      return Http::FilterHeadersStatus::Continue;
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus HttpSampleDecoderFilter::decodeData(Buffer::Instance&, bool) {
  return Http::FilterDataStatus::Continue;
}

void HttpSampleDecoderFilter::setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace SampleFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
