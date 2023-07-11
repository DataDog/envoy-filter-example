#pragma once

#include <string>
#include <unordered_set>

#include "header_processor.h"

#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "envoy/common/exception.h"
#include "http-filter-example/http_filter.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SampleFilter {

class HttpSampleDecoderFilterConfig {
public:
  HttpSampleDecoderFilterConfig(const sample::Decoder& proto_config);

  const std::string& key() const { return key_; }
  const std::string& val() const { return val_; }

private:
  const std::string key_;
  const std::string val_;
};

using HttpSampleDecoderFilterConfigSharedPtr = std::shared_ptr<HttpSampleDecoderFilterConfig>;
using HeaderProcessorUniquePtr = std::unique_ptr<HeaderProcessor>;

class HttpSampleDecoderFilter : public Http::PassThroughDecoderFilter {
public:
  HttpSampleDecoderFilter(HttpSampleDecoderFilterConfigSharedPtr);
  virtual ~HttpSampleDecoderFilter() {}

  void onDestroy() override {}

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks&) override;

private:
  const HttpSampleDecoderFilterConfigSharedPtr config_;
  Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  bool error_ = false;

  // header processors
  // TODO: add one for response header processing once filter is converted to Encoder/Decoder
  std::vector<HeaderProcessorUniquePtr> request_header_processors_;

  // set of accepted operations
  std::unordered_set<std::string> operations_;

  const Http::LowerCaseString headerKey() const;
  const std::string headerValue() const;
  void setError() { error_ = true; }
  bool getError() const { return error_; };
};

// TODO: might need to throw an exception in the future
// class FilterException : public EnvoyException {
// public:
//   using EnvoyException::EnvoyException;
// };

} // namespace SampleFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
