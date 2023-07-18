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
                    // ENVOY_LOG_MISC(info, "condition found"); // TODO: remove debug

                    if (it + 1 == operation_expression.end()) {
                    return absl::InvalidArgumentError("empty condition provided");
                    }

                    setConditionProcessor(std::make_shared<ConditionProcessor>());
                    const absl::Status status = getConditionProcessor()->parseOperation(operation_expression, it+1); // pass everything after the "if"
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
            headers.addCopy(Http::LowerCaseString(key), header_val); // should never return an error
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

                if (it + 1 == operation_expression.end()) {
                    return absl::InvalidArgumentError("empty condition provided");
                }

                setConditionProcessor(std::make_shared<ConditionProcessor>());
                const absl::Status status = getConditionProcessor()->parseOperation(operation_expression, it+1); // pass everything after the "if"
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
        const std::string request_path = getPath();

        if (!condition_result) {
            return absl::OkStatus(); // do nothing because condition is false
        }

        // cast to RequestHeaderMap because setPath is only on request side
        Http::RequestHeaderMap* request_headers = static_cast<Http::RequestHeaderMap*>(&headers);
        
        // set path
        request_headers->setPath(request_path); // should never return an error

        return absl::OkStatus();
    }

    void SetBoolProcessor::setStringsToCompare(std::pair<absl::string_view, absl::string_view> strings_to_compare) {
        std::string first_string = std::string(strings_to_compare.first);
        std::string second_string = std::string(strings_to_compare.second);

        strings_to_compare_ = std::make_pair(first_string, second_string); 
    }

    absl::Status SetBoolProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::SET_BOOL_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-bool");
        }

        ENVOY_LOG_MISC(info, "dumping bool contents");
        for (const auto& str : operation_expression) {
            ENVOY_LOG_MISC(info, std::string(str));
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

    bool SetBoolProcessor::executeOperation() const {
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

        return result;
    }

    absl::Status ConditionProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        // TODO: validate start pointer
        const Utility::BooleanOperatorType start_type = Utility::StringToBooleanOperatorType(*start);

        ENVOY_LOG_MISC(info, "dumping condition processor parsing contents");
        for (const auto& str : operation_expression) {
            ENVOY_LOG_MISC(info, std::string(str));
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
                ENVOY_LOG_MISC(info, "adding negated operand:");
                ENVOY_LOG_MISC(info, std::string(*(it+1)));
                operands_.push_back(std::pair<absl::string_view, bool>(*(it+1), true));
                it += 2;
            } else {
                ENVOY_LOG_MISC(info, "adding operand:");
                ENVOY_LOG_MISC(info, std::string(*it));
                operands_.push_back(std::pair<absl::string_view, bool>(*it, false));
                it++;
            }
        }

        // TODO: remove debug statements
        // std::string debug = "second operand negated is " + std::to_string(operands_.at(1).second) + " , operator is " + std::to_string(int(operators_.at(0)));
        // return absl::InvalidArgumentError(debug);
        // return absl::OkStatus();

        // validate number of operands and operators
        return (operators_.size() == operands_.size() - 1) ? absl::OkStatus() : absl::InvalidArgumentError("invalid condition");
    }

    absl::Status ConditionProcessor::executeOperation(SetBoolProcessorMapSharedPtr set_bool_processors) {
        try {
            if (operands_.size() == 1) {
                // look up the bool
                ENVOY_LOG_MISC(info, std::string(operands_.at(0).first));
                condition_ = operands_.at(0).second ? !(set_bool_processors->at(std::string(operands_.at(0).first))->executeOperation()) : set_bool_processors->at(std::string(operands_.at(0).first))->executeOperation();
                return absl::OkStatus();
            }

            bool result;

            // evaluate first operation
            bool op1 = operands_.at(0).second ? !(set_bool_processors->at(std::string(operands_.at(0).first))->executeOperation()) : set_bool_processors->at(std::string(operands_.at(0).first))->executeOperation();
            bool op2 = operands_.at(1).second ? !(set_bool_processors->at(std::string(operands_.at(1).first))->executeOperation()) : set_bool_processors->at(std::string(operands_.at(1).first))->executeOperation();

            result = Utility::evaluateExpression(op1, operators_.at(0), op2);

            auto operators_it = operators_.begin() + 1;
            auto operands_it = operands_.begin() + 2;

            while (operators_it != operators_.end() && operands_it != operands_.end()) {
                bool op2 = (*operands_it).second ? !(set_bool_processors->at(std::string((*operands_it).first))->executeOperation()) : set_bool_processors->at(std::string((*operands_it).first))->executeOperation();
                result = Utility::evaluateExpression(result, *operators_it, op2);
                operators_it++;
                operands_it++;
            }

            condition_ = result;
            return absl::OkStatus();

        } catch (std::exception& e) {
            return absl::InvalidArgumentError("failed to process condition");
        }
    }

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy