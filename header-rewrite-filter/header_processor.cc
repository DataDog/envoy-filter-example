#include "header_processor.h"

#include "source/common/common/logger.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

    absl::Status HeaderProcessor::ConditionProcessorSetup(std::vector<absl::string_view>& condition_expression, std::vector<absl::string_view>::iterator start) {
        if (start == condition_expression.end()) {
            return absl::InvalidArgumentError("empty condition provided");
        }

        setConditionProcessor(std::make_shared<ConditionProcessor>());
        return getConditionProcessor()->parseOperation(condition_expression, start); // pass everything after the "if"
    }

    absl::Status SetHeaderProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::SET_HEADER_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-header");
        }

        // TODO: remove
        start++;

        // parse key and call setKey
        try {
            const absl::string_view key = operation_expression.at(2); // TODO: use start
            setKey(key);
        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing header key");
        }

        // parse values and call setVals
        try {
            std::vector<std::string> vals;
            for(auto it = operation_expression.begin() + 3; it != operation_expression.end(); ++it) {
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
        // if condition is null, return true
        bool result = true;
        if (getConditionProcessor()) {
            const absl::Status status = getConditionProcessor()->executeOperation(getConditionProcessor()->getBoolProcessors());
            if (status != absl::OkStatus()) {
                return status;
            }
            result = getConditionProcessor()->getConditionResult();
        }
        setCondition(result);

        return absl::OkStatus();
    }

    absl::Status SetHeaderProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors) {
        const ConditionProcessorSharedPtr condition_processor = getConditionProcessor();
        if (condition_processor) {
            condition_processor->setBoolProccessors(bool_processors);
        }
        const absl::Status status = evaluateCondition();
        if (status != absl::OkStatus()) {
            return status;
        }
        const bool condition_result = getCondition(); // whether the condition is true or false
        const std::string key = getKey();
        const std::vector<std::string>& header_vals = getVals();

        if (!condition_result) {
            return absl::OkStatus(); // do nothing because condition is false
        }
        
        // set header
        for (auto const& header_val : header_vals) {
            headers.appendCopy(Http::LowerCaseString(key), header_val); // should never return an error
        }

        return absl::OkStatus();
    }

    absl::Status SetPathProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::SET_PATH_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-path");
        }

        // TODO: remove
        start++;

        // parse path and call setPath
        try {
            absl::string_view request_path = operation_expression.at(2); // TODO: use start
            setPath(request_path);

            if (operation_expression.size() > 3) {
                auto it = operation_expression.begin() + 3;
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

    absl::Status SetPathProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors) {
        const ConditionProcessorSharedPtr condition_processor = getConditionProcessor();
        if (condition_processor) {
            condition_processor->setBoolProccessors(bool_processors);
        }
        const absl::Status status = evaluateCondition();
        if (status != absl::OkStatus()) {
            return status;
        }

        const bool condition_result = getCondition(); // whether the condition is true or false
        const std::string new_path = getPath();

        if (!condition_result) {
            return absl::OkStatus(); // do nothing because condition is false
        }

        // cast to RequestHeaderMap because setPath is only on request side
        Http::RequestHeaderMap* request_headers = static_cast<Http::RequestHeaderMap*>(&headers);
        
        // get path (includes query string)
        absl::string_view path = request_headers->path();

        const size_t offset = path.find_first_of("?"); // TODO: envoy implements this as ?#

        if (offset == absl::string_view::npos) { // no query string present
            request_headers->setPath(new_path); // should never return an error
            return absl::OkStatus();
        }

        const absl::string_view query_string = path.substr(offset, path.length() - offset);
        
        // set path, preserves query string
        request_headers->setPath(new_path + std::string(query_string)); // should never return an error

        return absl::OkStatus();
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

        // TODO: remove
        start++;

        try {
            absl::string_view bool_name = operation_expression.at(2); // TODO: use start
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

    absl::Status SetBoolProcessor::executeOperation() {
        const Utility::MatchType match_type = getMatchType();

        bool result;

        switch (match_type) {
            case Utility::MatchType::Exact:
                result = (getStringsToCompare().first == getStringsToCompare().second);
                break;
            case Utility::MatchType::Substr:
                break;
            // TODO: implement rest of the match cases
            default:
                result = false;
        }

        result_ = result;

        return absl::OkStatus();
    }

    absl::Status ConditionProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        // TODO: validate start pointer
        const Utility::BooleanOperatorType start_type = Utility::StringToBooleanOperatorType(*start);

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

        // validate number of operands and operators
        return (operators_.size() == operands_.size() - 1) ? absl::OkStatus() : absl::InvalidArgumentError("invalid condition");
    }

    absl::Status ConditionProcessor::executeOperation(SetBoolProcessorMapSharedPtr set_bool_processors) {
        try {
            absl::Status status;
            bool bool_var;
            SetBoolProcessorSharedPtr bool_processor = set_bool_processors->at(std::string(operands_.at(0).first));
            if (operands_.size() >= 1) { // this function should never be called if there are no operands
                // look up the bool in the map, evaluate the value of the bool, and store the result
                status = bool_processor->executeOperation();
                if (status != absl::OkStatus()) {
                    return status;
                }
                bool_var = bool_processor->getResult();
                if (operands_.size() == 1) {
                    condition_ = operands_.at(0).second ? !bool_var : bool_var;
                    return absl::OkStatus();
                }
            }

            bool result = bool_var;

            auto operators_it = operators_.begin();
            auto operands_it = operands_.begin() + 1;

            // continue evaluating the condition from left to right
            while (operators_it != operators_.end() && operands_it != operands_.end()) {
                bool_processor = set_bool_processors->at(std::string((*operands_it).first));
                status = bool_processor->executeOperation();
                if (status != absl::OkStatus()) {
                    return status;
                }
                bool_var = bool_processor->getResult();
                bool operand = (*operands_it).second ? !bool_var : bool_var;
                result = Utility::evaluateExpression(result, *operators_it, operand);
                operators_it++;
                operands_it++;
            }
            
            // store the result
            condition_ = result;

            return absl::OkStatus();

        } catch (std::exception& e) { // fails gracefully if a faulty map access occurs
            std::cout << e.what();
            return absl::InvalidArgumentError("failed to process condition");
        }
    }

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy