#include "header_processor.h"

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
            std::vector<std::string> vals;
            for(auto it = operation_expression.begin() + 3; it != operation_expression.end(); ++it) {
                vals.push_back(std::string{*it}); // could throw bad_alloc
            }
            setVals(vals);
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
        const std::vector<std::string>& header_vals = getVals();

        if (!condition_result) {
            return; // do nothing because condition is false
        }
        
        // set header
        for (auto const& header_val : header_vals) {
            headers.appendCopy(Http::LowerCaseString(key), header_val); // should never return an error
        }
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
        
        // set path
        request_headers->setPath(request_path); // should never return an error
    }


    void SetBoolProcessor::setStringsToCompare(std::pair<absl::string_view, absl::string_view> strings_to_compare) {
        std::string first_string = std::string(strings_to_compare.first);
        std::string second_string = std::string(strings_to_compare.second);

        strings_to_compare_ = std::make_pair(first_string, second_string); 
    }

    absl::Status SetBoolProcessor::parseOperation(std::vector<absl::string_view>& operation_expression) {
        if (operation_expression.size() < Utility::SET_BOOL_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-bool");
        }

        try {
            absl::string_view bool_name = operation_expression.at(2);
            setBoolName(bool_name);

            if (operation_expression.at(4) != "-m") {
                return absl::InvalidArgumentError("invalid match syntax");
            }

            const Utility::MatchType match_type = Utility::StringToMatchType(operation_expression.at(5));

            switch (match_type) {
                case Utility::MatchType::Exact:
                    {
                        std::pair<absl::string_view, absl::string_view> strings_to_compare(operation_expression.at(3), operation_expression.at(6));
                        setStringsToCompare(strings_to_compare);
                        break;
                    }
                // TODO: implement this
                // case Utility::MatchType::Substr:
                //     break;
                // case Utility::MatchType::Found:
                //     break;
                default:
                    return absl::InvalidArgumentError("invalid match type");
            }

        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing request path argument");
        }

        return absl::OkStatus();
    }

    bool SetBoolProcessor::executeOperation() const {
        const Utility::MatchType match_type = getMatchType();

        bool result;

        switch (match_type) {
            case Utility::MatchType::Exact:
                result = (getStringsToCompare().first == getStringsToCompare().second);
                break;
            // TODO: implement rest of the match cases
            default:
                result = false;
        }

        return result;
    }


} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
