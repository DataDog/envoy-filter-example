#include "header_processor.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

    SetHeaderProcessor::SetHeaderProcessor() {}

    absl::Status SetHeaderProcessor::parseOperation(std::vector<absl::string_view>& operation_expression) {
        // parse key and call setKey
        try {
            absl::string_view key = operation_expression.at(2);
            setKey(key);
        } catch (const std::exception& e) {
            // should never happen, range is checked in HTTP filter
            return absl::InvalidArgumentError("error parsing header key");
        }

        // parse values and call setVals
        try {
            std::vector<std::string> vals;
            for(auto it = operation_expression.begin() + 3; it != operation_expression.end(); ++it) {
                vals.push_back(std::string{*it}); // could throw bad_alloc
            }
            setVals(vals);
        } catch (const std::exception& e) {
            return absl::InvalidArgumentError("error parsing header values");
        }

        // parse condition expression and call evaluate conditions on the parsed expression
        absl::Status status = evaluateCondition();
        return status;
    }

    absl::Status SetHeaderProcessor::evaluateCondition() {
        setCondition(true);
        return absl::OkStatus();
    }

    void SetHeaderProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers) const {
        bool condition_result = getCondition(); // whether the condition is true or false
        const std::string key = getKey();
        const std::vector<std::string>& header_vals = getVals();

        if (!condition_result) {
            return; // do nothing because condition is false
        }
        
        // set header
        for (auto const& header_val : header_vals) {
            headers.addCopy(Http::LowerCaseString(key), header_val); // should never return an error
        }
    }

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
