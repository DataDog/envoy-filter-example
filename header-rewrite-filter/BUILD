load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
)
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

package(default_visibility = ["//visibility:public"])

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        ":header_rewrite_config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

api_proto_package()

envoy_cc_library(
    name = "header_rewrite_lib",
    srcs = ["header_rewrite.cc"],
    hdrs = ["header_rewrite.h"],
    repository = "@envoy",
    deps = [
        ":pkg_cc_proto",
        ":header_rewrite_processor_lib",
        ":header_rewrite_utils_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
        "@envoy//source/common/common:utility_lib",
        "@envoy//source/common/common:minimal_logger_lib",
    ],
)

envoy_cc_library(
    name = "header_rewrite_config",
    srcs = ["header_rewrite_config.cc"],
    repository = "@envoy",
    deps = [
        ":header_rewrite_lib",
        "@envoy//envoy/server:filter_config_interface",
    ],
)

envoy_cc_library(
    name = "header_rewrite_processor_lib",
    srcs = ["header_processor.cc"],
    hdrs = [ "header_processor.h"],
    repository = "@envoy",
    deps = [
        ":pkg_cc_proto",
        ":header_rewrite_utils_lib",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "header_rewrite_utils_lib",
    srcs = ["utility.cc"],
    hdrs = ["utility.h",],
    repository = "@envoy",
    deps = [
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_test(
    name = "header_rewrite_integration_test",
    srcs = ["header_rewrite_integration_test.cc"],
    repository = "@envoy",
    deps = [
        ":header_rewrite_config",
        "@envoy//test/integration:http_integration_lib",
    ],
)
