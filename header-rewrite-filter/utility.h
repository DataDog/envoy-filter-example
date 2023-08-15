#pragma once

#include "absl/strings/string_view.h"
#include "absl/status/status.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {
namespace Utility {

constexpr absl::string_view HEADER_REWRITE_FILTER_NAME = "envoy.extensions.filters.http.HeaderRewrite";

constexpr uint8_t MIN_NUM_ARGUMENTS = 2;
constexpr uint8_t SET_HEADER_MIN_NUM_ARGUMENTS = 4;
constexpr uint8_t APPEND_HEADER_MIN_NUM_ARGUMENTS = 4;
constexpr uint8_t SET_DYN_METADATA_MIN_NUM_ARGUMENTS = 4;
constexpr uint8_t SET_PATH_MIN_NUM_ARGUMENTS = 3;
constexpr uint8_t SET_BOOL_MIN_NUM_ARGUMENTS = 6;
constexpr uint8_t DYN_FUNCTION_MIN_LENGTH = 3;

constexpr absl::string_view DYNAMIC_FUNCTION_DELIMITER = "%[]";

constexpr absl::string_view OPERATION_SET_HEADER = "set-header";
constexpr absl::string_view OPERATION_APPEND_HEADER = "append-header";
constexpr absl::string_view OPERATION_SET_PATH = "set-path";
constexpr absl::string_view OPERATION_SET_BOOL = "set-bool";
constexpr absl::string_view OPERATION_SET_DYN_METADATA = "set-metadata";

constexpr absl::string_view IF_KEYWORD = "if";

constexpr absl::string_view HTTP_REQUEST = "http-request";
constexpr absl::string_view HTTP_RESPONSE = "http-response";

constexpr absl::string_view MATCH_TYPE_EXACT = "str";
constexpr absl::string_view MATCH_TYPE_PREFIX = "beg";
constexpr absl::string_view MATCH_TYPE_SUBSTR = "sub";
constexpr absl::string_view MATCH_TYPE_FOUND = "found";

constexpr absl::string_view BOOLEAN_AND = "and";
constexpr absl::string_view BOOLEAN_OR = "or";
constexpr absl::string_view BOOLEAN_NOT = "not";

constexpr absl::string_view DYNAMIC_VALUE_HDR = "hdr";
constexpr absl::string_view DYNAMIC_VALUE_URL_PARAM = "urlp";
constexpr absl::string_view DYNAMIC_VALUE_METADATA = "metadata";

enum class OperationType : int {
  SetHeader,
  AppendHeader,
  SetPath,
  SetBool,
  SetDynMetadata,
  InvalidOperation,
};

enum class MatchType : int {
  Exact,
  Prefix,
  Substr,
  Found,
  InvalidMatchType,
};

enum class BooleanOperatorType : int {
  And,
  Or,
  Not,
  None,
};

enum class FunctionType : int {
  GetHdr,
  Urlp,
  GetMetadata,
  Static,
  InvalidFunctionType
};

OperationType StringToOperationType(absl::string_view operation);
MatchType StringToMatchType(absl::string_view match);
BooleanOperatorType StringToBooleanOperatorType(absl::string_view bool_operator);
FunctionType StringToFunctionType(absl::string_view function);

bool isOperator(BooleanOperatorType operator_type);
bool isOR(BooleanOperatorType operator_type);
bool isBinaryOperator(BooleanOperatorType operator_type);
bool evaluateExpression(bool operand1, BooleanOperatorType operator_val, bool operand2);

bool requiresArgument(MatchType match_type);

} // namespace Utility
} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
