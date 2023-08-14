#include "header_processor.h"

#include "source/common/common/logger.h" // TODO: remove debugging lib

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderRewriteFilter {

    absl::Status HeaderProcessor::ConditionProcessorSetup(std::vector<absl::string_view>& condition_expression, std::vector<absl::string_view>::iterator start) {
        if (start == condition_expression.end()) {
            return absl::InvalidArgumentError("empty condition provided");
        }
        setConditionProcessor(std::make_shared<ConditionProcessor>(bool_processors_, is_request_));
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
            const absl::string_view val = *(start + 1);
            setVal(val);

            if ((start + 2) != operation_expression.end()) {
                if (*(start + 2) == Utility::IF_KEYWORD) { // condition found
                    const absl::Status status = HeaderProcessor::ConditionProcessorSetup(operation_expression, start+3); // pass everything after the "if"

                    if (status != absl::OkStatus()) {
                        return status;
                    }
                }
                else {
                    return absl::InvalidArgumentError("third argument to set-header must be a condition");
                }
            }

        } catch (const std::exception& e) {
            return absl::InvalidArgumentError("error parsing header values -- " + std::string(e.what()));
        }
        return absl::OkStatus();
    }

    absl::Status AppendHeaderProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        if (operation_expression.size() < Utility::APPEND_HEADER_MIN_NUM_ARGUMENTS) {
            return absl::InvalidArgumentError("not enough arguments for append-header");
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

    std::tuple<absl::Status, bool> HeaderProcessor::evaluateCondition(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo) {
        // call ConditionProcessor executeOperation; if it is null, return true
        ConditionProcessorSharedPtr condition_processor = getConditionProcessor();
        if (condition_processor) {
            const std::tuple<absl::Status, bool> condition = condition_processor->executeOperation(headers, streamInfo);
            const absl::Status status = std::get<0>(condition);
            if (status != absl::OkStatus()) {
                return std::make_tuple(status, false);
            }
            return std::make_tuple(absl::OkStatus(), std::get<1>(condition)); // return condition result
        }


        return std::make_tuple(absl::OkStatus(), true); // no condition present
    }

    absl::Status SetHeaderProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo) {
        const std::tuple<absl::Status, bool> condition_result = evaluateCondition(headers, streamInfo);
        const absl::Status status = std::get<0>(condition_result);
        if (status != absl::OkStatus()) {
            return status;
        }
        const absl::string_view key = getKey();
        const std::string header_val(getVal());

        if (!std::get<1>(condition_result)) {
            return absl::OkStatus(); // do nothing because condition is false
        }
        
        // set header
        headers.setCopy(Http::LowerCaseString(key), header_val); // should never return an error


        return absl::OkStatus();
    }


    absl::Status AppendHeaderProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo) {
        const std::tuple<absl::Status, bool> condition_result = evaluateCondition(headers, streamInfo);
        const absl::Status status = std::get<0>(condition_result);
        if (status != absl::OkStatus()) {
            return status;
        }
        const absl::string_view key = getKey();
        const std::vector<std::string>& header_vals = getVals();

        if (!std::get<1>(condition_result)) {
            return absl::OkStatus(); // do nothing because condition is false
        }
        
        // append header
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

    absl::Status SetPathProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo) {
        const std::tuple<absl::Status, bool> condition_result = evaluateCondition(headers, streamInfo);
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
        try {
            if (operation_expression.size() < Utility::SET_BOOL_MIN_NUM_ARGUMENTS) {
                return absl::InvalidArgumentError("not enough arguments for set-bool");
            }

            if (*(start + 2) != "-m") {
                return absl::InvalidArgumentError("invalid match syntax");
            }
            const Utility::MatchType match_type = Utility::StringToMatchType(*(start + 3));

            // validate number of arguments
            if (Utility::requiresArgument(match_type)) {
                if (operation_expression.size() == Utility::SET_BOOL_MIN_NUM_ARGUMENTS) {
                    return absl::InvalidArgumentError("missing argument for match type");
                }
                if (operation_expression.size() > (Utility::SET_BOOL_MIN_NUM_ARGUMENTS + 1)) {
                    return absl::InvalidArgumentError("too many arguments for set bool");
                }
            } else if (operation_expression.size() > Utility::SET_BOOL_MIN_NUM_ARGUMENTS) { // if match type does not have an arg
                return absl::InvalidArgumentError("too many arguments for set bool");
            }

            // parse dynamic function
            dynamic_function_processor_ = std::make_shared<DynamicFunctionProcessor>(bool_processors_, is_request_);
            const absl::string_view dynamic_function_expression = *(start + 1);
            const absl::Status dynamic_function_status = dynamic_function_processor_->parseOperation(dynamic_function_expression);
            if (dynamic_function_status != absl::OkStatus()) {
                return dynamic_function_status;
            }

            switch (match_type) {
                case Utility::MatchType::Exact:
                {
                    std::string string_to_compare = std::string(*(start + 4));
                    matcher_ = [string_to_compare](std::string source) { return source.length() > 0 && source.compare(string_to_compare) == 0; };
                    break;
                }
                case Utility::MatchType::Prefix:
                {
                    std::string string_to_compare = std::string(*(start + 4));
                    matcher_ = [string_to_compare](std::string source) { return source.length() > 0 && string_to_compare.find(source.c_str(), 0, string_to_compare.size()) == 0; };
                    break;
                }
                case Utility::MatchType::Substr:
                {
                    std::string string_to_compare = std::string(*(start + 4));
                    matcher_ = [string_to_compare](std::string source) { return source.length() > 0 && source.find(string_to_compare) != std::string::npos; };
                    break;
                }
                case Utility::MatchType::Found: // urlp found or hdr found
                {
                    matcher_ = [](std::string source) { return source.length() > 0; };
                    break;
                }
                default:
                    return absl::InvalidArgumentError("invalid match type");
            }

        } catch (const std::exception& e) { // could throw bad optional access if config syntax is wrong
            return absl::InvalidArgumentError("error parsing boolean expression -- " + std::string(e.what()));
        }

        return absl::OkStatus();
    }

    std::tuple<absl::Status, bool> SetBoolProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo, bool negate) {
        const std::tuple<absl::Status, std::string> result = dynamic_function_processor_->executeOperation(headers, streamInfo);
        const absl::Status status = std::get<0>(result);
        const std::string source = std::get<1>(result);

        if (status != absl::OkStatus()) {
            return std::make_tuple(status, false);
        }

        const bool bool_result = matcher_(source);
        const bool apply_negation = negate ? !bool_result : bool_result;

        return std::make_tuple(absl::OkStatus(), apply_negation);
    }

    absl::Status ConditionProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
        // make sure operands and operators vectors are empty
        operands_ = {};
        operators_ = {};

        Utility::BooleanOperatorType start_type;
        if (operation_expression.size() < 1) {
            return absl::InvalidArgumentError("empty condition provided to condition processor");
        }
        start_type = Utility::StringToBooleanOperatorType(*start);

        // conditional can't start with a binary operator
        if (Utility::isBinaryOperator(start_type)) {
            return absl::InvalidArgumentError("invalid condition -- condition must begin with 'not' or an operand");
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

                // make sure that all the boolean variables being referenced exist in the map
                const auto operand = *(it+1);
                if (bool_processors_->find(std::string(operand)) == bool_processors_->end()) {
                    return absl::InvalidArgumentError("boolean variable \"" + std::string(operand) + "\" in conditional does not exist");
                }

                operands_.push_back(std::tuple<std::string, bool>(*(it+1), true));
                it += 2;
            } else {
                // make sure that all the boolean variables being referenced exist in the map
                const auto operand = *(it);
                if (bool_processors_->find(std::string(operand)) == bool_processors_->end()) {
                    return absl::InvalidArgumentError("boolean variable \"" + std::string(operand) + "\" in conditional does not exist");
                }

                operands_.push_back(std::tuple<std::string, bool>(*it, false));
                it++;
            }
        }

        // validate number of operands and operators
        return (operators_.size() == (operands_.size() - 1)) ? absl::OkStatus() : absl::InvalidArgumentError("invalid condition");
    }

    // return status and condition result
    std::tuple<absl::Status, bool> ConditionProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo) {
        try {
            const SetBoolProcessorSharedPtr first_bool_processor = bool_processors_->at(std::string(std::get<0>(operands_.at(0))));
            // look up the bool in the map, evaluate the value of the bool, and store the result
            const std::tuple<absl::Status, bool> bool_var_result = first_bool_processor->executeOperation(headers, streamInfo, std::get<1>(operands_.at(0)));
            const absl::Status status = std::get<0>(bool_var_result);
            if (status != absl::OkStatus()) {
                return std::make_tuple(status, false);
            }
            const bool bool_var = std::get<1>(bool_var_result);
            if (operands_.size() == 1) {
                return std::make_tuple(absl::OkStatus(), bool_var);
            }

            bool result = bool_var;

            auto operators_it = operators_.begin();
            auto operands_it = operands_.begin() + 1;

            // continue evaluating the condition from left to right
            while (operators_it != operators_.end() && operands_it != operands_.end()) {
                const SetBoolProcessorSharedPtr next_bool_processor = bool_processors_->at(std::string(std::get<0>((*operands_it))));

                const std::tuple<absl::Status, bool> bool_var_result = next_bool_processor->executeOperation(headers, streamInfo, std::get<1>(*operands_it));
                const absl::Status status = std::get<0>(bool_var_result);
                if (status != absl::OkStatus()) {
                    return std::make_tuple(status, false);
                }

                const bool bool_var = std::get<1>(bool_var_result);
                result = Utility::evaluateExpression(result, *operators_it, bool_var);
                operators_it++;
                operands_it++;
            }

            return std::make_tuple(absl::OkStatus(), result);

        } catch (std::exception& e) { // fails gracefully if a faulty map access occurs
            return std::make_tuple(absl::InvalidArgumentError("failed to process condition -- " + std::string(e.what())), false);
        }
    }

  std::tuple<absl::Status, std::string> DynamicFunctionProcessor::getFunctionArgument(absl::string_view function_expression) {
    try {
        auto start = function_expression.find_first_of("(");
        auto end = function_expression.find_last_of(")");
        if (end != (function_expression.size()-1)) {
            return std::make_tuple(absl::InvalidArgumentError("failed to get function argument -- invalid dynamic function syntax"), "");
        }
        std::string argument = std::string(function_expression).substr(start+1, end-start-1);
        return std::make_tuple(absl::OkStatus(), argument);
    } catch (std::exception& e) {
        return std::make_tuple(absl::InvalidArgumentError("failed to get function argument"), "");
    }
  }

  Utility::FunctionType DynamicFunctionProcessor::getFunctionType(absl::string_view function_expression) {
    const auto end = function_expression.find_first_of("(");
    const auto dynamic_function = function_expression.substr(0, end);
    return Utility::StringToFunctionType(dynamic_function);
  }

  absl::Status DynamicFunctionProcessor::parseOperation(absl::string_view function_expression) {
    if (function_expression.length() < Utility::DYN_FUNCTION_MIN_LENGTH) {
        return absl::InvalidArgumentError("invalid syntax for dynamic function -- function too short");
    }

    // make sure dynamic function is wrapped in %[]
    if (function_expression.substr(0, 2) != Utility::DYNAMIC_FUNCTION_DELIMITER.substr(0, 2) && function_expression.substr(function_expression.size()-1, 1) != Utility::DYNAMIC_FUNCTION_DELIMITER.substr(2, 1)) {
        // TODO: treat it as a static value if it's not wrapped in %[]
        return absl::InvalidArgumentError("dynamic function must be wrapped with %[]");
    }
    function_type_ = getFunctionType(function_expression.substr(2, function_expression.size() - Utility::DYNAMIC_FUNCTION_DELIMITER.size()));
    if (function_type_ == Utility::FunctionType::InvalidFunctionType) {
        return absl::InvalidArgumentError("invalid function type for dynamic value");
    }
    if (function_type_ == Utility::FunctionType::Urlp && !is_request_) {
        return absl::InvalidArgumentError("cannot get url path parameter on response side");
    }
    const std::tuple<absl::Status, std::string> get_function_argument_result = getFunctionArgument(function_expression.substr(2, function_expression.size() - Utility::DYNAMIC_FUNCTION_DELIMITER.size()));
    const absl::Status status = std::get<0>(get_function_argument_result);
    if (status != absl::OkStatus()) {
        return status;
    }
    function_argument_ = std::get<1>(get_function_argument_result);

    // validate dynamic function arguments
    const auto arguments = StringUtil::splitToken(function_argument_, ",", false, true);
    switch (function_type_) {
        case Utility::FunctionType::GetHdr:
        {
            if (arguments.size() < 1 || arguments.size() > 2) {
                return absl::InvalidArgumentError("wrong number of arguments to get header function");
            }
            break;
        }
        case Utility::FunctionType::Urlp:
        {
            if (arguments.size() != 1) {
                return absl::InvalidArgumentError("wrong number of arguments arguments to get urlp function");
            }
            break;
        }
        default:
            return absl::InvalidArgumentError("invalid function type for dynamic value function");
    }

    return absl::OkStatus();
  }

  std::tuple<absl::Status, std::string> DynamicFunctionProcessor::getHeaderValue(Http::RequestOrResponseHeaderMap& headers, absl::string_view key, int position) {
    try {
        const Http::LowerCaseString header_key(key);
        const Envoy::Http::HeaderUtility::GetAllOfHeaderAsStringResult header = Envoy::Http::HeaderUtility::getAllOfHeaderAsString(headers, header_key);

        if (header.result() == absl::nullopt) { // header does not exist
            return std::make_tuple(absl::OkStatus(), "");
        }
        const absl::string_view values_string_view = header.result().value();
        const auto header_vals = StringUtil::splitToken(values_string_view, ",", false, true);
        const auto num_header_vals = header_vals.size();

        if (position < 0) {
            position += num_header_vals;
        }

        // validate position
        if ((position < 0) || (position >= num_header_vals)) {
            return std::make_tuple(absl::InvalidArgumentError("invalid match syntax -- hdr position out of bounds"), "");
        }

        // get comma-separated value of the header
        const auto header_val = header_vals.at(position);
        const std::string source(header_val);

        return std::make_tuple(absl::OkStatus(), source);
    } catch (std::exception& e) { // should never happen, bounds are checked above
        return std::make_tuple(absl::InvalidArgumentError("failed to perform boolean match -- " + std::string(e.what())), "");
    }
}

  std::tuple<absl::Status, std::string> DynamicFunctionProcessor::getUrlp(Http::RequestOrResponseHeaderMap& headers, absl::string_view param) {
    try {
        Http::RequestHeaderMap* request_headers = dynamic_cast<Http::RequestHeaderMap*>(&headers); // can fail if invalid config is provided, ie if response tries to get path
        if (!request_headers) {
            return std::make_tuple(absl::InvalidArgumentError("cannot call urlp function on response side"), "");
        }
        const auto query_parameters = Http::Utility::parseQueryString(request_headers->Path()[0].value().getStringView());
        const auto& iter = query_parameters.find(std::string(param));
        if (iter == query_parameters.end()) { // query param doesn't exist
            return std::make_tuple(absl::OkStatus(), "");
        }
        
        const std::string source(iter->second);
        return std::make_tuple(absl::OkStatus(), source);
    } catch (std::exception& e) { // should never happen, bounds are checked above
        return std::make_tuple(absl::InvalidArgumentError("failed to perform boolean match" + std::string(e.what())), "");
    }
}

  std::tuple<absl::Status, std::string> DynamicFunctionProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo) {
    const auto arguments = StringUtil::splitToken(function_argument_, ",", false, true);
    std::tuple<absl::Status, std::string> result;
    std::string source;
    switch (function_type_) {
        case Utility::FunctionType::GetHdr:
        {
            int position;
            const absl::string_view header_key = arguments.at(0);
            if (arguments.size() == 1) {
                position = -1;
            } else {
                position = std::stoi(std::string(arguments.at(1)));
            }
            result = getHeaderValue(headers, header_key, position);
            const absl::Status status = std::get<0>(result);
            source = std::get<1>(result);
            return std::make_tuple(absl::OkStatus(), source);
        }
        case Utility::FunctionType::Urlp:
        {
            result = getUrlp(headers, arguments.at(0));
            const absl::Status status = std::get<0>(result);
            source = std::get<1>(result);
            return std::make_tuple(absl::OkStatus(), source);
        }
        default:
            break;
    }
    return std::make_tuple(absl::InvalidArgumentError("failed to execute dynamic function -- invalid function type"), "");
  }

  absl::Status SetDynamicMetadataProcessor::parseOperation(std::vector<absl::string_view>& operation_expression, std::vector<absl::string_view>::iterator start) {
    if (operation_expression.size() < Utility::SET_DYN_METADATA_MIN_NUM_ARGUMENTS) {
        return absl::InvalidArgumentError("not enough arguments for set-metadata");
    }

    try {
        metadata_key_ = std::string(*start);
        const absl::string_view value = *(start + 1);
        metadata_value_ = std::make_shared<DynamicFunctionProcessor>(bool_processors_, is_request_);
        const absl::Status parse_dynamic_function_status = metadata_value_->parseOperation(value);

        if (parse_dynamic_function_status != absl::OkStatus()) {
            return parse_dynamic_function_status;
        }

        if (start + 2 != operation_expression.end()) {
            if (*(start + 2) == Utility::IF_KEYWORD) { // condition found
                const absl::Status parse_condition_status = HeaderProcessor::ConditionProcessorSetup(operation_expression, start+3); // pass everything after the "if"
                if (parse_condition_status != absl::OkStatus()) {
                    return parse_condition_status;
                }
            } else {
                return absl::InvalidArgumentError("third argument to set-metadata must be a condition");
            }
        }


    } catch (const std::exception& e) {
        // should never happen, range is checked above
        return absl::InvalidArgumentError("error parsing set metadata operation -- " + std::string(e.what()));
    }

    return absl::OkStatus();
  }

  absl::Status SetDynamicMetadataProcessor::executeOperation(Http::RequestOrResponseHeaderMap& headers, Envoy::StreamInfo::StreamInfo* streamInfo) {
    try {
        const std::tuple<absl::Status, bool> condition_result = evaluateCondition(headers, streamInfo);
        const absl::Status condition_status = std::get<0>(condition_result);
        if (condition_status != absl::OkStatus()) {
            return condition_status;
        }

        if (!std::get<1>(condition_result)) {
            return absl::OkStatus(); // do nothing because condition is false
        }

        // get dynamic value to set
        const std::tuple<absl::Status, std::string> metadata_value_result = metadata_value_->executeOperation(headers, streamInfo);
        const absl::Status metadata_value_status = std::get<0>(metadata_value_result);
        const std::string value = std::get<1>(metadata_value_result);
        if (metadata_value_status != absl::OkStatus()) {
            return absl::InvalidArgumentError("failed to get dynamic value to set metadata -- " + std::string(metadata_value_status.message()));
        }
        if (value.length() == 0) {
            return absl::InvalidArgumentError("failed to get dynamic value to set metadata -- no value");
        }

        if (!streamInfo) {
            return absl::InvalidArgumentError("streamInfo is null");
        }

        // set metadata
        envoy::config::core::v3::Metadata& dynamic_metadata = streamInfo->dynamicMetadata();
        ProtobufWkt::Struct metadata( // get metadata for header rewrite filter
            (*dynamic_metadata.mutable_filter_metadata())[std::string(Utility::HEADER_REWRITE_FILTER_NAME)]);
        auto& fields = *metadata.mutable_fields();
        auto val = ProtobufWkt::Value();
        val.set_string_value(value);

        // we have to erase the key if it exists, otherwise map insert will do nothing
        if (fields.find(metadata_key_) != fields.end()) {
            fields.erase(metadata_key_);
        }
 
        fields.insert({metadata_key_, val});
        streamInfo->setDynamicMetadata(std::string(Utility::HEADER_REWRITE_FILTER_NAME), metadata);
        
        return absl::OkStatus();
    } catch (std::exception& e) {
        return absl::InvalidArgumentError("failed to set metadata -- " + std::string(e.what()));
    }
  }

} // namespace HeaderRewriteFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy