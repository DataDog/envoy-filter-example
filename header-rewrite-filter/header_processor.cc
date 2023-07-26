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

        // parse key and call setKey
        try {
            const absl::string_view key = *start;
            setKey(key);
        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing header key -- " + std::string(e.what()));
        }

        // parse values and call setVals
        try {
            std::vector<std::string> vals;
            for(auto it = start + 1; it != operation_expression.end(); ++it) {
                if (*it == Utility::IF_KEYWORD) { // condition found

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
            return absl::InvalidArgumentError("error parsing header values -- " + std::string(e.what()));
        }

        return absl::OkStatus();
    }

    absl::Status AppendHeaderProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::APPEND_HEADER_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-header");
        }

        // parse key and call setKey
        try {
            const absl::string_view key = *start;
            setKey(key);
        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing header key -- " + std::string(e.what()));
        }

        // parse values and call setVals
        try {
            std::vector<std::string> vals;
            for(auto it = start + 1; it != operation_expression.end(); ++it) {
                if (*it == Utility::IF_KEYWORD) { // condition found

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
            return absl::InvalidArgumentError("error parsing header values -- " + std::string(e.what()));
        }

        return absl::OkStatus();
    }

    // return status and condition result
    std::tuple<absl::Status, bool> HeaderProcessor::evaluateCondition(SetBoolProcessorMapSharedPtr bool_processors) {
        // call ConditionProcessor executeOperation; if it is null, return true
        ConditionProcessorSharedPtr condition_processor = getConditionProcessor();
        if (condition_processor) {
            const std::tuple<absl::Status, bool> condition = condition_processor->executeOperation(bool_processors);
            const absl::Status status = std::get<0>(condition);
            if (status != absl::OkStatus()) {
                return std::make_tuple(status, false);
            }
            return std::make_tuple(absl::OkStatus(), std::get<1>(condition)); // return condition result
        }

        return std::make_tuple(absl::OkStatus(), true); // no condition present
    }

    absl::Status SetHeaderProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors) {
       const std::tuple<absl::Status, bool> condition_result = evaluateCondition(bool_processors);
        const absl::Status status = std::get<0>(condition_result);
        if (status != absl::OkStatus()) {
            return status;
        }
        const absl::string_view key = getKey();
        const std::vector<std::string>& header_vals = getVals();

        if (!std::get<1>(condition_result)) {
            return absl::OkStatus(); // do nothing because condition is false
        }
        
        // set header
        for (auto const& header_val : header_vals) {
            headers.appendCopy(Http::LowerCaseString(key), header_val); // should never return an error
        }

        return absl::OkStatus();
    }


    absl::Status AppendHeaderProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors) {
        const std::tuple<absl::Status, bool> condition_result = evaluateCondition(bool_processors);
        const absl::Status status = std::get<0>(condition_result);
        if (status != absl::OkStatus()) {
            return status;
        }
        const absl::string_view key = getKey();
        const std::vector<std::string>& header_vals = getVals();

        if (!std::get<1>(condition_result)) {
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

        // parse path and call setPath
        try {
            absl::string_view request_path = *start;
            setPath(request_path);

            if (operation_expression.size() > 3) {
                auto it = start + 1;
                if (*it != Utility::IF_KEYWORD) {
                    return absl::InvalidArgumentError("second argument to set-path must be a condition");
                }

                const absl::Status status = HeaderProcessor::ConditionProcessorSetup(operation_expression, it+1); // pass everything after the "if"

                if (status != absl::OkStatus()) {
                    return status;
                }
            }
        } catch (const std::exception& e) {
            // should never happen, range is checked above
            return absl::InvalidArgumentError("error parsing request path argument -- " + std::string(e.what()));
        }

        return absl::OkStatus();
    }

    absl::Status SetPathProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, SetBoolProcessorMapSharedPtr bool_processors) {
        const std::tuple<absl::Status, bool> condition_result = evaluateCondition(bool_processors);
        const absl::Status status = std::get<0>(condition_result);
        if (status != absl::OkStatus()) {
            return status;
        }

        const std::string new_path = getPath();

        if (!std::get<1>(condition_result)) {
            return absl::OkStatus(); // do nothing because condition is false
        }

        // cast to RequestHeaderMap because setPath is only on request side
        Http::RequestHeaderMap* request_headers = static_cast<Http::RequestHeaderMap*>(&headers);
        
        // get path (includes query string)
        absl::string_view path = request_headers->path();

        const size_t offset = path.find_first_of("?");

        if (offset == absl::string_view::npos) { // no query string present
            request_headers->setPath(new_path); // should never return an error
            return absl::OkStatus();
        }

        const absl::string_view query_string = path.substr(offset, path.length() - offset);

        // set path, preserves query string
        request_headers->setPath(new_path + std::string(query_string)); // should never return an error

        return absl::OkStatus();
    }

    absl::Status SetBoolProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::SET_BOOL_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for set-bool");
        }

        try {
            if (*(start + 2) != "-m") {
                return absl::InvalidArgumentError("invalid match syntax");
            }

            const Utility::MatchType match_type = Utility::StringToMatchType(*(start + 3));

            std::string string_to_compare;

            switch (match_type) {
                case Utility::MatchType::Exact:
                    string_to_compare = std::string(*(start + 4));
                    source_ = *(start + 1);
                    matcher_ = [string_to_compare](std::string source) { return (source.compare(string_to_compare) == 0); };
                    break;
                // TODO: implement this
                case Utility::MatchType::Substr:
                    break;
                case Utility::MatchType::Found:
                    break;
                default:
                    return absl::InvalidArgumentError("invalid match type");
            }

        } catch (const std::exception& e) {
            return absl::InvalidArgumentError("error parsing boolean expression -- " + std::string(e.what()));
        }

        return absl::OkStatus();
    }

    // return status and bool result
    std::tuple<absl::Status, bool> SetBoolProcessor::executeOperation(bool negate) {
        try {
            const bool result = matcher_(source_);
            const bool apply_negation = negate ? !result : result;
            return std::make_pair(absl::OkStatus(), apply_negation);
        } catch (std::exception& e) {
            return std::make_pair(absl::InvalidArgumentError("failed to perform boolean match -- " + std::string(e.what())), false);
        }
    }

    absl::Status ConditionProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        Utility::BooleanOperatorType start_type;
        try {
            start_type = Utility::StringToBooleanOperatorType(*start);
        } catch (std::exception& e) {
            return absl::InvalidArgumentError("failed to parse condition -- " + std::string(e.what()));
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
                operands_.push_back(std::tuple<std::string, bool>(*(it+1), true));
                it += 2;
            } else {
                operands_.push_back(std::tuple<std::string, bool>(*it, false));
                it++;
            }
        }

        // validate number of operands and operators
        return (operators_.size() == operands_.size() - 1) ? absl::OkStatus() : absl::InvalidArgumentError("invalid condition");
    }

    // return status and condition result
    std::tuple<absl::Status, bool> ConditionProcessor::executeOperation(SetBoolProcessorMapSharedPtr set_bool_processors) {
        try {
            std::tuple<absl::Status, bool> bool_var_result;
            absl::Status status;
            bool bool_var;

            SetBoolProcessorSharedPtr bool_processor = set_bool_processors->at(std::string(std::get<0>(operands_.at(0))));
            if (operands_.size() >= 1) { // this function should never be called if there are no operands
                // look up the bool in the map, evaluate the value of the bool, and store the result
                bool_var_result = bool_processor->executeOperation(std::get<1>(operands_.at(0)));
                status = std::get<0>(bool_var_result);
                if (status != absl::OkStatus()) {
                    return std::make_tuple(status, false);
                }
                bool_var = std::get<1>(bool_var_result);
                if (operands_.size() == 1) {
                    return std::make_tuple(absl::OkStatus(), bool_var);
                }
            }

            bool result = bool_var;

            auto operators_it = operators_.begin();
            auto operands_it = operands_.begin() + 1;

            // continue evaluating the condition from left to right
            while (operators_it != operators_.end() && operands_it != operands_.end()) {
                bool_processor = set_bool_processors->at(std::string(std::get<0>((*operands_it))));

                bool_var_result = bool_processor->executeOperation(std::get<1>(operands_.at(0)));
                status = std::get<0>(bool_var_result);
                if (status != absl::OkStatus()) {
                    return std::make_tuple(status, false);
                }

                bool_var = std::get<1>(bool_var_result);
                result = Utility::evaluateExpression(result, *operators_it, bool_var);
                operators_it++;
                operands_it++;
            }

            return std::make_tuple(absl::OkStatus(), result);

        } catch (std::exception& e) { // fails gracefully if a faulty map access occurs
            return std::make_tuple(absl::InvalidArgumentError("failed to process condition" + std::string(e.what())), false);
        }
    }

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy