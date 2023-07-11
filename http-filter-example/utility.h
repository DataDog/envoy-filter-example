#pragma once

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SampleFilter {
namespace Utility {

constexpr uint8_t MIN_NUM_ARGUMENTS = 3;

constexpr absl::string_view HTTP_REQUEST = "http-request";
constexpr absl::string_view HTTP_RESPONSE = "http-response";

enum class OperationType : int {
  SetHeader,
  SetPath,
  InvalidOperation,
};

OperationType StringToOperationType(absl::string_view operation);

} // namespace Utility
} // namespace SampleFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy