#include "utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {
namespace Utility {

 OperationType StringToOperationType(absl::string_view operation) {
    if (operation == "set-header") {
        return OperationType::SetHeader;
    } else if (operation == "set-path") {
        return OperationType::SetPath;
    } else if (operation == "set-bool") {
        return OperationType::SetBool;
    } else {
        return OperationType::InvalidOperation;
    }
}

MatchType StringToMatchType(absl::string_view match) {
   if (match == MATCH_TYPE_EXACT) {
        return MatchType::Exact;
    } else if (match == MATCH_TYPE_SUBSTR) {
        return MatchType::Substr;
    } else if (match == MATCH_TYPE_FOUND) {
        return MatchType::Found;
    } else {
        return MatchType::InvalidMatchType;
    }
}

FunctionType StringToFunctionType(absl::string_view function) {
    if (function.find(DYNAMIC_VALUE_REQ_HDR) == 0 || function.find(DYNAMIC_VALUE_RES_HDR) == 0) {
        return FunctionType::GetHdr;
    } else if (function.find(DYNAMIC_VALUE_URL_PARAM) == 0) {
        return FunctionType::Urlp;
    } else {
        return FunctionType::InvalidFunctionType;
    }
}

absl::Status GetFunctionArgument(FunctionType function_type, absl::string_view& function_expression){
    try {
        auto start = function_expression.find_first_of("(");
        auto end = function_expression.find_last_of(")");
        function_expression = function_expression.substr(start+1, function_expression.size()-start-2);
        return absl::OkStatus();
    } catch (std::exception& e) {
        return absl::InvalidArgumentError("failed to get function argument");
    }
}

} // namespace Utility
} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy