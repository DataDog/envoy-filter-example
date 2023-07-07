#include "header_processor.h"

namespace Envoy {
namespace Http {

    SetHeaderProcessor::SetHeaderProcessor() {}

    int SetHeaderProcessor::parseOperation(std::vector<absl::string_view>& operation_expression) {
        int err = 0;

        if (operation_expression.size() < 4) {
            return 1;
        }

        // parse key and call setKey
        std::string key = std::string(operation_expression.at(2));
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

    int SetHeaderProcessor::executeOperation(RequestHeaderMap& headers) const {
        int err = 0; // TODO will executing set-header ever return an error?
        bool condition_result = getCondition(); // whether the condition is true or false
        const std::string key = getKey();
        const std::vector<std::string>& header_vals = getVals();

        if (!condition_result) {
            // do nothing because condition is false
            return err;
        }
        
        // set header
        for (auto const& header_val : header_vals) {
            headers.addCopy(LowerCaseString(key), header_val);
        }

        return err;
    }

} // namespace Http
} // namespace Envoy
