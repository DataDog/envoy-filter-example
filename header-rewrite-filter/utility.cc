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

} // namespace Utility
} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy