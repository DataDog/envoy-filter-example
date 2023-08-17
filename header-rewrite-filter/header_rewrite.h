#pragma once

#include <string>

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

  const std::string& config() const { return config_; }

private:
  const std::string config_;
};

using HttpHeaderRewriteFilterConfigSharedPtr = std::shared_ptr<HttpHeaderRewriteFilterConfig>;
using HeaderProcessorUniquePtr = std::unique_ptr<HeaderProcessor>;
using SetBoolProcessorSharedPtr = std::shared_ptr<SetBoolProcessor>;
using SetBoolProcessorMapSharedPtr = std::shared_ptr<std::unordered_map<std::string, SetBoolProcessorSharedPtr>>;

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

  // set_bool processors
  SetBoolProcessorMapSharedPtr request_set_bool_processors_;
  SetBoolProcessorMapSharedPtr response_set_bool_processors_;

  const std::string headerConfig() const;
  void setError() { error_ = true; }
  bool getError() const { return error_; };
};

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
