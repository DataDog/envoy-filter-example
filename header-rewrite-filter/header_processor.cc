#include "header_processor.h"
#include "utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

    absl::Status SetHeaderProcessor::parseOperation(std::vector<absl::string_view>& operation_expression) {
        if (operation_expression.size() < Utility::SET_HEADER_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-header");
        }

        // parse key and call setKey
        try {
            const absl::string_view key = operation_expression.at(2);
            setKey(key);
        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing header key");
        }

        // parse values and call setVals
        try {
            setVal(std::string(*(operation_expression.begin() + 3)));
        } catch (const std::exception& e) {
            return absl::InvalidArgumentError("error parsing header values");
        }

        // parse condition expression and call evaluate conditions on the parsed expression
        const absl::Status status = evaluateCondition();
        return status;
    }

    absl::Status SetHeaderProcessor::evaluateCondition() {
        setCondition(true);
        return absl::OkStatus();
    }

    void SetHeaderProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers) const {
        bool condition_result = getCondition(); // whether the condition is true or false
        const std::string key = getKey();
        const std::string header_val = getVal();

        if (!condition_result) {
            return; // do nothing because condition is false
        }
        
        // set header
        headers.setCopy(Http::LowerCaseString(key), header_val); // should never return an error
    }

     absl::Status SetPathProcessor::evaluateCondition() {
        setCondition(true);
        return absl::OkStatus();
    }

    absl::Status SetPathProcessor::parseOperation(std::vector<absl::string_view>& operation_expression) {
        if (operation_expression.size() < Utility::SET_PATH_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-path");
        }

        // parse path and call setPath
        try {
            absl::string_view request_path = operation_expression.at(2);
            setPath(request_path);
        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing request path argument");
        }

        // parse condition expression and call evaluate conditions on the parsed expression
        const absl::Status status = evaluateCondition();
        return status;
    }

    void SetPathProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers) const {
        const bool condition_result = getCondition(); // whether the condition is true or false
        const std::string request_path = getPath();

        if (!condition_result) {
            return; // do nothing because condition is false
        }

        // cast to RequestHeaderMap because setPath is only on request side
        Http::RequestHeaderMap* request_headers = static_cast<Http::RequestHeaderMap*>(&headers);
        
        // set path (path being set here includes the query string)
        request_headers->setPath(request_path); // should never return an error
    }

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
