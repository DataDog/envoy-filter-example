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
    if (bool_operator == "and") {
        return BooleanOperatorType::And;
    } else if (bool_operator == "or") {
        return BooleanOperatorType::Or;
    } else if (bool_operator == "not") {
        return BooleanOperatorType::Not;
    } else {
        return BooleanOperatorType::None;
    }
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


} // namespace Utility
} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy