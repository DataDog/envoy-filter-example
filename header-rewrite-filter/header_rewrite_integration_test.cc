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
  void SetUp() override {}
  void SetUp(std::string config) { initialize(config); }

  void initialize() override {}
  void initialize(std::string config) {
    config_helper_.prependFilter(config);
    HttpIntegrationTest::initialize();
  }

};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpFilterHeaderRewriteIntegrationTest,
                         testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

// test adding a new header key-value pair
TEST_P(HttpFilterHeaderRewriteIntegrationTest, SetHeader) {
  SetUp("{ name: sample, typed_config: { \"@type\": type.googleapis.com/envoy.extensions.filters.http.HeaderRewrite, key: header-processing,"
    "val: http-request set-header sample_key sample_value } }");
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
      "sample_value",
      request_stream->headers().get(Http::LowerCaseString("sample_key"))[0]->value().getStringView());

  codec_client->close();
}

// test adding a new value to an already existing header
TEST_P(HttpFilterHeaderRewriteIntegrationTest, AppendHeader) {
  SetUp("{ name: sample, typed_config: { \"@type\": type.googleapis.com/envoy.extensions.filters.http.HeaderRewrite, key: header-processing,"
    "val: http-request append-header sample_key sample_value2 sample_value3 } }");
  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/"}, {":authority", "host"}, {"sample_key", "sample_value1"}};
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
      "sample_value1,sample_value2,sample_value3",
      request_stream->headers().get(Http::LowerCaseString("sample_key"))[0]->value().getStringView());

  codec_client->close();
}

// test set-metadata operation
TEST_P(HttpFilterHeaderRewriteIntegrationTest, SetMetadata) {
  SetUp("{ name: sample, typed_config: { \"@type\": type.googleapis.com/envoy.extensions.filters.http.HeaderRewrite, key: header-processing,"
    "\"val\": \"http-request set-metadata mock_key %[hdr(mock_header)]\" } }");
  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/"}, {":authority", "host"}, {"mock_header", "mock_value"}};
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

  const auto filter_metadata_map = request_stream->streamInfo().dynamicMetadata().filter_metadata();
  
  // verify metadata for the filter exists
  EXPECT_TRUE(filter_metadata_map.find("envoy.extensions.filters.http.HeaderRewrite") != filter_metadata_map.end());

  const auto metadata_map = filter_metadata_map.find("envoy.extensions.filters.http.HeaderRewrite")->second.fields();

  // verify metadata with the expected key exists
  EXPECT_TRUE(metadata_map.find("mock_key") != metadata_map.end());

  // verify expected value of metadata
  EXPECT_EQ("mock_value", metadata_map.find("mock_key")->second.string_value());

  codec_client->close();
}
} // namespace Envoy