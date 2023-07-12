#include "test/integration/http_integration.h"

namespace Envoy {
class HttpFilterHeaderRewriteIntegrationTest : public HttpIntegrationTest,
                                        public testing::TestWithParam<Network::Address::IpVersion> {
public:
  HttpFilterHeaderRewriteIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()) {}
  /**
   * Initializer for an individual integration test.
   */
  void SetUp() override { }
  void SetUp(std::string config) { initialize(config); }

  void initialize() override {
    config_helper_.prependFilter(
        "{ name: sample, typed_config: { \"@type\": type.googleapis.com/sample.Decoder, key: via, "
        "val: sample-filter } }");
    HttpIntegrationTest::initialize();
  }

  void initialize(std::string config) {
    config_helper_.prependFilter(config);
    HttpIntegrationTest::initialize();
  }

};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpFilterHeaderRewriteIntegrationTest,
                         testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

TEST_P(HttpFilterHeaderRewriteIntegrationTest, Test1) {
  SetUp("{ name: sample, typed_config: { \"@type\": type.googleapis.com/envoy.extensions.filters.http.HeaderRewrite, key: header-processing,"
    "val: http-request set-header x-forwarded-proto https } }");
  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/"}, {":authority", "host"}};
  Http::TestRequestHeaderMapImpl response_headers{
      {":status", "200"}};

  IntegrationCodecClientPtr codec_client;
  FakeHttpConnectionPtr fake_upstream_connection;
  FakeStreamPtr request_stream;

  codec_client = makeHttpConnection(lookupPort("http"));
  auto response = codec_client->makeHeaderOnlyRequest(headers);
  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection));
  ASSERT_TRUE(fake_upstream_connection->waitForNewStream(*dispatcher_, request_stream));
  ASSERT_TRUE(request_stream->waitForEndStream(*dispatcher_));
  request_stream->encodeHeaders(response_headers, true);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_EQ(
      "http,https",
      request_stream->headers().get(Http::LowerCaseString("x-forwarded-proto"))[0]->value().getStringView());

  codec_client->close();
}
} // namespace Envoy
