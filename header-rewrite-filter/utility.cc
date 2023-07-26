#include "utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {
namespace Utility {

 OperationType StringToOperationType(absl::string_view operation) {
    if (operation == OPERATION_SET_HEADER) {
        return OperationType::SetHeader;
    } else if (operation == OPERATION_APPEND_HEADER) {
        return OperationType::AppendHeader;
    } else if (operation == OPERATION_SET_PATH) {
        return OperationType::SetPath;
    } else if (operation == OPERATION_SET_BOOL) {
        return OperationType::SetBool;
    } else {
        return OperationType::InvalidOperation;
    }
}

MatchType StringToMatchType(absl::string_view match) {
   if (match == MATCH_TYPE_EXACT) {
        return MatchType::Exact;
    } else if (match == MATCH_TYPE_PREFIX) {
        return MatchType::Prefix;
    } else if (match == MATCH_TYPE_SUBSTR) {
        return MatchType::Substr;
    } else if (match == MATCH_TYPE_FOUND) {
        return MatchType::Found;
    } else {
        return MatchType::InvalidMatchType;
    }
}

BooleanOperatorType StringToBooleanOperatorType(absl::string_view bool_operator) {
    if (bool_operator == BOOLEAN_AND) {
        return BooleanOperatorType::And;
    } else if (bool_operator == BOOLEAN_OR) {
        return BooleanOperatorType::Or;
    } else if (bool_operator == BOOLEAN_NOT) {
        return BooleanOperatorType::Not;
    } else {
        return BooleanOperatorType::None;
    }
}

FunctionType StringToFunctionType(absl::string_view function) {
    if (function.compare(DYNAMIC_VALUE_HDR) == 0) {
        return FunctionType::GetHdr;
    } else if (function.compare(DYNAMIC_VALUE_URL_PARAM) == 0) {
        return FunctionType::Urlp;
    } else {
        return FunctionType::InvalidFunctionType;
    }
}

std::tuple<absl::Status, std::string> GetFunctionArgument(absl::string_view function_expression){
    try {
        auto start = function_expression.find_first_of("(");
        auto end = function_expression.find_last_of(")");
        std::string argument = std::string(function_expression).substr(start+1, function_expression.size()-start-2);
        return std::make_tuple(absl::OkStatus(), argument);
    } catch (std::exception& e) {
        return std::make_tuple(absl::InvalidArgumentError("failed to get function argument"), "");
    }
}
FunctionType GetFunctionType(absl::string_view function_expression){
    const auto end = function_expression.find_first_of("(");
    const auto dynamic_function = function_expression.substr(0, end);
    return StringToFunctionType(dynamic_function);
}

bool isOperator(BooleanOperatorType operator_type) {
    return (operator_type == BooleanOperatorType::And || operator_type == BooleanOperatorType::Or || operator_type == BooleanOperatorType::Not);
}

bool isBinaryOperator(BooleanOperatorType operator_type) {
    return (operator_type == BooleanOperatorType::And || operator_type == BooleanOperatorType::Or);
}

bool evaluateExpression(bool operand1, BooleanOperatorType operator_val, bool operand2) {
    if (operator_val == BooleanOperatorType::And) return operand1 && operand2;
    return operand1 || operand2;
}

bool requiresArgument(MatchType match_type) {
    return (match_type == MatchType::Exact || match_type == MatchType::Substr || match_type == MatchType::Prefix);
}

std::tuple<absl::Status, std::string> getHeaderValue(Http::RequestOrResponseHeaderMap& headers, std::vector<absl::string_view> arguments) {
    try {
        const Http::LowerCaseString header_key(arguments.at(0));
        const Envoy::Http::HeaderUtility::GetAllOfHeaderAsStringResult header = Envoy::Http::HeaderUtility::getAllOfHeaderAsString(headers, header_key);

        if (header.result() == absl::nullopt) { // header does not exist
            return std::make_tuple(absl::OkStatus(), "");
        }
        const absl::string_view values_string_view = header.result().value();
        const auto header_vals = StringUtil::splitToken(values_string_view, ",", false, true);
        const auto num_header_vals = header_vals.size();

        // get header value based on an optional position argument, if no position specified return the last value
        if (arguments.size() > 2) {
            return std::make_tuple(absl::InvalidArgumentError("invalid match syntax -- too many arguments to hdr function"), "");
        }

        int index;
        if (arguments.size() == 1) { // no position specified
            index = num_header_vals - 1;
        } else { // position specified
            index = std::stoi(std::string(arguments.at(1)));
            if (index < 0) { // negative position specified
                index += num_header_vals;
            }
        }

        // validate index
        if ((index < 0) || (index >= num_header_vals)) {
            return std::make_tuple(absl::InvalidArgumentError("invalid match syntax -- hdr position out of bounds"), "");
        }

        // get comma-separated value of the header
        const auto header_val = header_vals.at(index);
        const std::string source(header_val);

        return std::make_tuple(absl::OkStatus(), source);
    } catch (std::exception& e) { // should never happen, bounds are checked above
        return std::make_tuple(absl::InvalidArgumentError("failed to perform boolean match -- " + std::string(e.what())), "");
    }
}

std::tuple<absl::Status, std::string> getUrlp(Http::RequestOrResponseHeaderMap& headers, std::vector<absl::string_view> arguments) {
    try {
        Http::RequestHeaderMap* request_headers = dynamic_cast<Http::RequestHeaderMap*>(&headers); // can fail if invalid config is provided, ie if response tries to get path
        if (!request_headers) {
            return std::make_tuple(absl::InvalidArgumentError("cannot call urlp function on response side"), "");
        }
        const auto query_parameters = Http::Utility::parseQueryString(request_headers->Path()[0].value().getStringView());
        const auto& iter = query_parameters.find(std::string(arguments.at(0)));
        if (iter == query_parameters.end()) { // query param doesn't exist
            return std::make_tuple(absl::OkStatus(), "");
        }
        
        const std::string source(iter->second);
        return std::make_tuple(absl::OkStatus(), source);
    } catch (std::exception& e) { // should never happen, bounds are checked above
        return std::make_tuple(absl::InvalidArgumentError("failed to perform boolean match" + std::string(e.what())), "");
    }
}

} // namespace Utility
} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
