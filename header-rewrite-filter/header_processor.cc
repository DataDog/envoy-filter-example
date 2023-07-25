#include "header_processor.h"

#include "source/common/common/logger.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

    absl::Status SetHeaderProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::SET_HEADER_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-header");
        }

        // parse key and call setKey
        try {
            const absl::string_view key = *start;
            setKey(key);
        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing header key");
        }

        // parse values and call setVals
        try {
            std::vector<std::string> vals;
            for(auto it = start + 1; it != operation_expression.end(); ++it) {
                if (*it == "if") { // condition found

                    const absl::Status status = HeaderProcessor::ConditionProcessorSetup(operation_expression, it+1); // pass everything after the "if"

                    if (status != absl::OkStatus()) {
                        return status;
                    }
                    break;
                }
                vals.push_back(std::string{*it}); // could throw bad_alloc
            }
            setVals(vals);
        } catch (const std::exception& e) {
            return absl::InvalidArgumentError("error parsing header values");
        }

        return absl::OkStatus();
    }

    absl::Status HeaderProcessor::evaluateCondition() {
        // call ConditionProcessor executeOperation; if it is null, return true
        bool result = true;
        if (getConditionProcessor()) {
            result = getConditionProcessor()->executeOperation();
        }
        setCondition(result);

        return absl::OkStatus();
    }

    absl::Status HeaderProcessor::ConditionProcessorSetup(std::vector<absl::string_view>& condition_expression, std::vector<absl::string_view>::iterator start) {
        if (start == condition_expression.end()) {
            return absl::InvalidArgumentError("empty condition provided");
        }

        setConditionProcessor(std::make_shared<ConditionProcessor>());
        return getConditionProcessor()->parseOperation(condition_expression, start); // pass everything after the "if"
    }

    void SetHeaderProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers) {
        evaluateCondition();
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

    absl::Status SetPathProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::SET_PATH_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-path");
        }

        // parse path and call setPath
        try {
            absl::string_view request_path = *start;
            setPath(request_path);

            if (operation_expression.size() > 3) {
                auto it = start + 1;
                if (*it != "if") {
                    return absl::InvalidArgumentError("second argument to set-path must be a condition");
                }

                const absl::Status status = HeaderProcessor::ConditionProcessorSetup(operation_expression, it+1); // pass everything after the "if"

                if (status != absl::OkStatus()) {
                    return status;
                }
            }
        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing request path argument");
        }

        return absl::OkStatus();
    }

    void SetPathProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers) {
        evaluateCondition();
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

    void SetBoolProcessor::setStringsToCompare(std::pair<absl::string_view, absl::string_view> strings_to_compare) {
        std::string first_string(strings_to_compare.first);
        std::string second_string(strings_to_compare.second);

        strings_to_compare_ = std::make_pair(first_string, second_string); 
    }

    absl::Status SetBoolProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::SET_BOOL_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-bool");
        }

        try {
            absl::string_view bool_name = *start;
            setBoolName(bool_name);

            if (*(start + 2) != "-m") {
                return absl::InvalidArgumentError("invalid match syntax");
            }

            const Utility::MatchType match_type = Utility::StringToMatchType(*(start + 3));

            switch (match_type) {
                case Utility::MatchType::Exact:
                    {
                        std::pair<absl::string_view, absl::string_view> strings_to_compare(*(start + 1), *(start + 4));
                        setStringsToCompare(strings_to_compare);
                        break;
                    }
                // TODO: implement this
                case Utility::MatchType::Substr:
                    break;
                case Utility::MatchType::Found:
                    break;
                default:
                    return absl::InvalidArgumentError("invalid match type");
            }

        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing request path argument");
        }

        return absl::OkStatus();
    }

    void SetBoolProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers) {
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

        result_ = result;
    }

    absl::Status ConditionProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        Utility::BooleanOperatorType start_type;
        try {
            start_type = Utility::StringToBooleanOperatorType(*start);
        } catch (std::exception& e) {
            return absl::InvalidArgumentError("failed to parse condition");
        }

        // conditional can't start with a binary operator
        if (Utility::isBinaryOperator(start_type)) {
            return absl::InvalidArgumentError("invalid condition -- condition must begin with '!' or an operand");
        }

        for (auto it = start; it != operation_expression.end();) {
            const Utility::BooleanOperatorType operator_type = Utility::StringToBooleanOperatorType(*it);

            // condition can't have two binary operators in a row
            if (it != start) {
                const Utility::BooleanOperatorType prev_operator_type = Utility::StringToBooleanOperatorType(*(it-1));
                if (Utility::isBinaryOperator(operator_type) && Utility::isBinaryOperator(prev_operator_type)) {
                    return absl::InvalidArgumentError("invalid condition -- cannot have two binary operators in a row");
                }
            }

            // condition can't end with an operator
            if (it+1 == operation_expression.end() && Utility::isOperator(operator_type)) {
                return absl::InvalidArgumentError("invalid condition -- cannot have an operator at the end of the condition");
            }

            // parse operation type
            if (Utility::isBinaryOperator(operator_type)) {
                operators_.push_back(operator_type);
                it++;
            } else if (operator_type == Utility::BooleanOperatorType::Not) {
                if (it + 1 == operation_expression.end()) {
                    return absl::InvalidArgumentError("invalid condition -- must have operand after 'not'");
                } else if (Utility::isOperator(Utility::StringToBooleanOperatorType(*(it+1)))) {
                    return absl::InvalidArgumentError("invalid condition -- can't have an operator after 'not'");
                }
                operands_.push_back(std::pair<absl::string_view, bool>(*(it+1), true));
                it += 2;
            } else {
                operands_.push_back(std::pair<absl::string_view, bool>(*it, false));
                it++;
            }
        }

        // TODO: validate number of operands and operators
        return absl::OkStatus();
    }

    bool ConditionProcessor::executeOperation() {
        if (operands_.size() == 1) {
            return operands_.at(0).second ? false : true; // TODO: remove mock value
        }

        bool result = true;

        // evaluate first operation
        result = Utility::evaluateExpression(operands_.at(0), operators_.at(0), operands_.at(1));

        auto operators_it = operators_.begin() + 1;
        auto operands_it = operands_.begin() + 2;

        while (operators_it != operators_.end() && operands_it != operands_.end()) {
            result = Utility::evaluateExpression(result, *operators_it, *operands_it);
            operators_it++;
            operands_it++;
        }

        return result;
    }

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
