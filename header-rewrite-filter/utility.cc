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
    } else {
        return OperationType::InvalidOperation;
    }
}

} // namespace Utility
} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy