#pragma once

#include <string>
#include <unordered_set>

#include "header_processor.h"

#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "envoy/common/exception.h"
#include "http-filter-example/http_filter.pb.h"

namespace Envoy {
namespace Http {

class HttpSampleDecoderFilterConfig {
public:
  HttpSampleDecoderFilterConfig(const sample::Decoder& proto_config);

  const std::string& key() const { return key_; }
  const std::string& val() const { return val_; }
  int extra() const { return extra_; }

private:
  const std::string key_;
  const std::string val_;
  const int extra_ = 0;
};

using HttpSampleDecoderFilterConfigSharedPtr = std::shared_ptr<HttpSampleDecoderFilterConfig>;

class HttpSampleDecoderFilter : public PassThroughDecoderFilter {
public:
  HttpSampleDecoderFilter(HttpSampleDecoderFilterConfigSharedPtr);
  ~HttpSampleDecoderFilter();

  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(RequestHeaderMap&, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks&) override;

private:
  const HttpSampleDecoderFilterConfigSharedPtr config_;
  StreamDecoderFilterCallbacks* decoder_callbacks_;
  int error_ = 0;

  // header processors
  // TODO: add one for response header processing once filter is converted to Encoder/Decoder
  std::vector<HeaderProcessor*> request_header_processors_;

  // set of accepted operations
  std::unordered_set<std::string> operations_;

  const LowerCaseString headerKey() const;
  const std::string headerValue() const;
  int headerExtra() const;
  void setError(const int val);
  int getError() const;
};

// TODO: might need to throw an exception in the future
// class FilterException : public EnvoyException {
// public:
//   using EnvoyException::EnvoyException;
// };

} // namespace Http
} // namespace Envoy
