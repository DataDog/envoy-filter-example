#include "utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SampleFilter {
namespace Utility {

 OperationType StringToOperationType(absl::string_view operation) {
    if (operation == "set-header") {
        return OperationType::kSetHeader;
    } else if (operation == "set-path") {
        return OperationType::kSetPath;
    } else {
        return OperationType::kInvalidOperation;
    }
}

} // namespace Utility
} // namespace SampleFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy