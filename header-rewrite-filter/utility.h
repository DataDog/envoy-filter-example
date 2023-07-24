#pragma once

#include "absl/strings/string_view.h"
#include "absl/status/status.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {
namespace Utility {

constexpr uint8_t MIN_NUM_ARGUMENTS = 2;
constexpr uint8_t SET_HEADER_MIN_NUM_ARGUMENTS = 4;
constexpr uint8_t SET_PATH_MIN_NUM_ARGUMENTS = 3;
constexpr uint8_t SET_BOOL_MIN_NUM_ARGUMENTS = 6;

constexpr absl::string_view HTTP_REQUEST = "http-request";
constexpr absl::string_view HTTP_RESPONSE = "http-response";
constexpr absl::string_view HTTP_REQUEST_RESPONSE = "http";

constexpr absl::string_view MATCH_TYPE_EXACT = "str";
constexpr absl::string_view MATCH_TYPE_SUBSTR = "sub";
constexpr absl::string_view MATCH_TYPE_FOUND = "found";

constexpr absl::string_view DYNAMIC_VALUE_REQ_HDR = "req.hdr";
constexpr absl::string_view DYNAMIC_VALUE_RES_HDR = "res.hdr";
constexpr absl::string_view DYNAMIC_VALUE_URL_PARAM = "urlp";

enum class OperationType : int {
  SetHeader,
  SetPath,
  SetBool,
  InvalidOperation,
};

enum class MatchType : int {
  Exact,
  Substr,
  Found,
  InvalidMatchType,
};

enum class FunctionType : int {
  GetHdr,
  Urlp,
  InvalidFunctionType
};

OperationType StringToOperationType(absl::string_view operation);
MatchType StringToMatchType(absl::string_view match);
FunctionType StringToFunctionType(absl::string_view function);

absl::Status GetFunctionArgument(FunctionType function_type, absl::string_view& function_expression);

} // namespace Utility
} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy