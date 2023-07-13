#include <string>

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

#include "header-rewrite-filter/header_rewrite.pb.h"
#include "header-rewrite-filter/header_rewrite.pb.validate.h"
#include "header_rewrite.h"


namespace Envoy {
namespace Server {
namespace Configuration {

class HttpHeaderRewriteFilterConfigFactory : public NamedHttpFilterConfigFactory {
public:
  Http::FilterFactoryCb createFilterFactoryFromProto(const Protobuf::Message& proto_config,
                                                     const std::string&,
                                                     FactoryContext& context) override {

    return createFilter(Envoy::MessageUtil::downcastAndValidate<const envoy::extensions::filters::http::HeaderRewrite&>(
                            proto_config, context.messageValidationVisitor()),
                        context);
  }

  /**
   *  Return the Protobuf Message that represents your config incase you have config proto
   */
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new envoy::extensions::filters::http::HeaderRewrite()};
  }

  std::string name() const override { return "sample"; }

private:
  Http::FilterFactoryCb createFilter(const envoy::extensions::filters::http::HeaderRewrite& proto_config, FactoryContext&) {
    Extensions::HttpFilters::HeaderRewriteFilter::HttpHeaderRewriteFilterConfigSharedPtr config =
        std::make_shared<Extensions::HttpFilters::HeaderRewriteFilter::HttpHeaderRewriteFilterConfig>(
            Extensions::HttpFilters::HeaderRewriteFilter::HttpHeaderRewriteFilterConfig(proto_config));

    return [config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto filter = new Extensions::HttpFilters::HeaderRewriteFilter::HttpHeaderRewriteFilter(config);
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
    };
  }
};

/**
 * Static registration for this sample filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<HttpHeaderRewriteFilterConfigFactory, NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
 