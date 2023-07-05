#pragma once

#include <string>
#include <unordered_set>

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

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(RequestHeaderMap&, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks&) override;

private:
  const HttpSampleDecoderFilterConfigSharedPtr config_;
  StreamDecoderFilterCallbacks* decoder_callbacks_;
  int error_ = 0;

  // set of accepted operations
  std::unordered_set<std::string> operations_;

  //key is header operation, value is a vector of arguments for the operation
  std::unordered_map<std::string, std::vector<std::string>> header_ops_;

  const LowerCaseString headerKey() const;
  const std::string headerValue() const;
  int headerExtra() const;
  void setError(const int val);
  int getError() const;
};

class FilterException : public EnvoyException {
public:
  using EnvoyException::EnvoyException;
};

} // namespace Http
} // namespace Envoy
