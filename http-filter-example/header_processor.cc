#include "header_processor.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SampleFilter {

    SetHeaderProcessor::SetHeaderProcessor() {}

    int SetHeaderProcessor::parseOperation(std::vector<absl::string_view>& operation_expression) {
        int err = 0;

        if (operation_expression.size() < 4) {
            return 1;
        }

        // parse key and call setKey
        absl::string_view key = operation_expression.at(2);
        setKey(key);

        // parse values and call setVals
        std::vector<std::string> vals;
        for(auto it = operation_expression.begin() + 3; it != operation_expression.end(); ++it) {
            vals.push_back(std::string{*it});
        }
        setVals(vals);

        // parse condition expression and call evaluate conditions on the parsed expression
        evaluateCondition();

        return err;
    }

    int SetHeaderProcessor::evaluateCondition() {
        int err = 0;
        setCondition(true);
        return err;
    }

    int SetHeaderProcessor::executeOperation(Http::RequestHeaderMap& headers) const {
        int err = 0; // TODO will executing set-header ever return an error?
        bool condition_result = getCondition(); // whether the condition is true or false
        const std::string key = getKey();
        const std::vector<std::string>& header_vals = getVals();

        if (!condition_result) {
            return err; // do nothing because condition is false
        }
        
        // set header
        for (auto const& header_val : header_vals) {
            headers.addCopy(Http::LowerCaseString(key), header_val);
        }

        return err;
    }

} // namespace SampleFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
