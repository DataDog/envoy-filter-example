#pragma once

#include <string>
#include <unordered_set>

#include "header_processor.h"

#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "envoy/common/exception.h"
#include "header-rewrite-filter/header_rewrite.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

class HttpHeaderRewriteFilterConfig {
public:
  HttpHeaderRewriteFilterConfig(const envoy::extensions::filters::http::HeaderRewrite& proto_config);

  const std::string& key() const { return key_; }
  const std::string& val() const { return val_; }

private:
  const std::string key_;
  const std::string val_;
};

using HttpHeaderRewriteFilterConfigSharedPtr = std::shared_ptr<HttpHeaderRewriteFilterConfig>;
using HeaderProcessorUniquePtr = std::unique_ptr<HeaderProcessor>;

class HttpHeaderRewriteFilter : public Http::PassThroughFilter {
public:
  HttpHeaderRewriteFilter(HttpHeaderRewriteFilterConfigSharedPtr);
  virtual ~HttpHeaderRewriteFilter() {}

  void onDestroy() override {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool) override;

private:
  const HttpHeaderRewriteFilterConfigSharedPtr config_;
  bool error_ = false;

  // header processors
  std::vector<HeaderProcessorUniquePtr> request_header_processors_;
  std::vector<HeaderProcessorUniquePtr> response_header_processors_;

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

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
