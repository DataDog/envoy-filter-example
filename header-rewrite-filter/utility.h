#pragma once

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {
namespace Utility {

constexpr uint8_t MIN_NUM_ARGUMENTS = 2;
constexpr uint8_t SET_HEADER_MIN_NUM_ARGUMENTS = 4;
constexpr uint8_t SET_PATH_MIN_NUM_ARGUMENTS = 3;

constexpr absl::string_view HTTP_REQUEST = "http-request";
constexpr absl::string_view HTTP_RESPONSE = "http-response";
constexpr absl::string_view HTTP_REQUEST_RESPONSE = "http";

enum class OperationType : int {
  SetHeader,
  SetPath,
  SetBool,
  InvalidOperation,
};

enum class MatchType : int {
  Exact,
  Substr,
  InvalidMatchType,
};

OperationType StringToOperationType(absl::string_view operation);
MatchType StringToMatchType(absl::string_view match);

} // namespace Utility
} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy