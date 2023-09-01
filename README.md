# Envoy Header Rewrite Filter Documentation
## Introduction
### Envoy and HTTP Filters
One of Envoy’s very useful features is its ability to hook into the L7 network layer using [HTTP filters](https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/http/http_filters). HTTP filters are essentially Envoy modules that can be added to the request and/or response path to carry out a network behavior. HTTP filters operate on HTTP messages without knowledge of the underlying protocol, making it easy to inspect and modify state at this network layer. HTTP filters are also modular and ordered – they can programmatically be enabled and are invoked in a specific order along a path – which is very convenient for mixing and matching different filters to achieve the desired network behaviors. [Here](https://www.envoyproxy.io/docs/envoy/v1.27.0/configuration/http/http) is a list of all the HTTP filters that Envoy has developed.

There are three types of HTTP filters: decoder filters (active on the request path), encoder filters (active on the response path), decoder/encoder filters (active on both paths). Below is an example HTTP filter chain illustrating which filters are activated along each path and in which order they are activated:

 ![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/a06f4023-aa56-4db7-86e9-0079ab71553a)
 
From the Envoy docs: “HTTP filters can stop and continue iteration to subsequent filters. This allows for more complex scenarios such as health check handling, calling a rate limiting service, buffering, routing, generating statistics for application traffic such as DynamoDB, etc. HTTP level filters can also share state (static and dynamic) among themselves within the context of a single request stream. Refer to [data sharing between filters](https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/advanced/data_sharing_between_filters#arch-overview-data-sharing-between-filters) for more details.”

## Motivation
This filter improves the developer experience and performance of performing HTTP header rewrites.
- [The Lua filter](https://www.envoyproxy.io/docs/envoy/latest/configuration/http/http_filters/lua_filter), an HTTP filter that can also perform header rewrites, requires writing lots of code to perform even very simple header rewrites.
- The Lua filter invalidates the route cache when request headers are modified, which has a performance impact that can be avoided.
- The Lua filter’s functionality is limited to the APIs that are implemented within the filter. While there are currently a good amount of Envoy [wrappers](https://github.com/envoyproxy/envoy/blob/b4e61389f7db842a7bcee89d0fc080b23b27a9d2/source/extensions/filters/http/lua/wrappers.h) exposed within the Lua filter, there are still more that have not yet been implemented, such as information about retransmits, bytes received, etc. A custom filter is thus more extensible, as any desired functionality can be added at any time.
## Implementation
The Header Rewrite Filter executes as part of the HTTP filter chain managed by the HTTP Connection Manager, which is a [network filter](https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/listeners/listener_filters#arch-overview-network-filters). It follows a parse-execute structure.

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/19832af7-c763-47b4-a8a1-2fe98524c98b)

### Parse
The Header Rewrite filter protobuf struct looks like this:
```
message HeaderRewrite {
  string config = 1 [(validate.rules).string.min_len = 1];
}
```
At initialization time, the filter takes a single string value as its configuration. This string value should contain a list of header rewrite operations with each operation separated by a newline. When parsing the `config`, the filter first splits the string by newline (i.e. by operation) and parses each operation one at a time. For each operation, the filter will construct a `Processor` object that carries out the parse-execute sequence for that operation. The input to the Processor’s parse function is a vector of `string_view`'s, which is simply the operation split by spaces. (Spaces are thus special characters and should not be used unless specified).

At this step, a `Processor` will do the following:

- Check for syntax errors and other errors (such as a nonexistent boolean variable being referenced in the operation)

- Check for a condition within the operation. If a condition is found (designated by the keyword `if`), a `Processor` will initialize a ConditionProcessor and direct it to parse the conditional expression following the `if`.

- Store all relevant information such as arguments to the operation, arguments to dynamic functions being called within the function, operators/operands from the conditional expression

The Header Rewrite filter keeps a vector of request-side `Processors` and response-side `Processor`s. It checks the value of the first token in each operation, which should be either `http-request` or `http-response`, to determine what vector to append the `Processor` to. It will append the `Processor` to the proper vector after directing that `Processor` to parse its operation. (Note: `BoolProcessors` are stored separately, as they are not executing in the main request/response path and are instead invoked each time a boolean variable is referenced in an operation’s condition).

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/26004bbf-3f51-4011-894d-fc1bbf84faf9)

### Execute
When a request or a response comes in, the Header Rewrite filter’s specialized callback is triggered. On the request path, `decodeHeaders` is called. On the response path, `encodeHeaders` is called. In the triggered callback, the Header Rewrite filter will go through the list of `Processor`s and direct each one to execute its respective operation.

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/3109e0de-c716-40c2-bf11-49f7a2716799)

### Processor Class Structure
All processors are derived from the `Processor` base class, which contains the base functionality common to all processors (eg. parsing, whether the operation is request or response side). A `HeaderProcessor` is a second base class which implements hooking a conditional expression, supported by a `ConditionProcessor`, into an operation (this is probably not the best name for the base class and would be better named as `ProcessorWithCondition` or something similar).

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/fe70ac72-9779-4e36-a7e1-42ab4ae76a6c)

### Conditions
A `ConditionProcessor` is a member of HeaderProcessor. When parsing a header operation, if the `if` keyword is used a condition is detected. `HeaderProcessor` will [set up](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.h#L97) and parse the `ConditionProcessor`. The condition is [evaluated](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.cc#L121) in each `HeaderProcessor`’s `executeOperation`. Conditions are made of boolean variables, which are declared as `set-bool` operations in the filter's config. Each boolean operation is processed in its own `SetBoolProcessor`. When a condition is being evaluated, it must look up the value of a boolean variable. It uses the `bool_processors_` map (which is a member variable of all `Processor`'s) to look up the `SetBoolProcessor` for that boolean variable and calls the `SetBoolProcessor`’s `executeOperation`. Thus, dynamic values such as header values, URL parameters, and metadata are fetched at execution time and are consistent with the latest header rewrite operations that have been applied to the request/response.

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/9bd957c3-7eda-403c-97dd-33db433d25ad)

## DSL
A header rewrite command follows this general structure:

`<active path> <operation type> <arguments> if <conditional expression>`

Things to keep in mind:

- Because the filter’s parse step will split each operation by a space character, spaces are special characters and must not be used anywhere other than the positions specified in the syntax for each operation. Extra spaces in the positions specified are tolerated but should be avoided.

- `if` is a keyword and thus cannot be used as a string literal.

- Numbers are treated like strings.
### Operations

#### Set Header
`<http-request/http-response> set-header <key> <value> if <conditional expression>`

- `key` or `value` can be a static string literal or the result of a dynamic function (see related section below)

- if `key` does not exist a new header will be created, otherwise if a header already exists with the given `key`, its value will be replaced with value
#### Append Header
`<http-request/http-response> append-header <key> <values> if <conditional expression>`

- `key` or `value` can be a static string literal or the result of a dynamic function (see related section below)

- if `key` does not exist a new header will be created, otherwise value will be appended to the header as a comma-delimited list (eg. `{"key" : "value1,value2,value3"}`)
#### Set Path
`<http-request/http-response> set-path <path> if <conditional expression>`

- `path` can be a static string literal or the result of a dynamic function (see related section below)

- this operation preserves the query string (eg. `/oldpath?apikey=1` → `/newpath?apikey=1`)
#### Set Metadata
`<http-request/http-response> set-metadata <key> <value> if <conditional expression>`

[Dynamic metadata](https://www.envoyproxy.io/docs/envoy/latest/configuration/advanced/well_known_dynamic_metadata) is a way for different HTTP filters to share data – it can sometimes be useful to set certain values that can be accessed at a later point in time in the current filter or another filter in the chain, or for setting a value during request time and accessing that value at response time.

- `key` or `value` can be a static string literal or the result of a dynamic function (see related section below)

- if metadata already exists, it will be replace with the new value
### Conditional Expressions
Conditional expressions are a sequence of boolean variables, which must be declared using the `set-bool` operation. These boolean variables can be negated with `not` and are joined by either an `and` or an `or`. `and`’s are always evaluated before `or`’s.
#### Set Bool
`<http-request/http-response> set-bool <bool name> <source> -m <match type> <optional arg>`

- `source` or `optional arg` can be a static string literal or the result of a dynamic function (see related section below)
### Match Types
#### Exact
Whether `source` or `comparison`, either of which can be string literals or the result of a dynamic function, are the same value.

`<source> -m str <comparison>`
#### Prefix
Whether `comparison` is a prefix of `source`. `source` and `comparison` can be string literals or the result of a dynamic function.

`<source> -m beg <comparison>`
#### Substring
Whether `comparison` is a prefix of `source`. `source` and `comparison` can be string literals or the result of a dynamic function.

`<source> -m sub <comparison>`
#### Found
Whether `source` exists. `source` can be a string literal or the result of a dynamic function.

`<source> -m found`
### Dynamic Functions
Whenever you want a `key`, `value`, `path`, etc. to be the result of a dynamic function, the function call must be wrapped in `%[]` so that the parser knows to treat the token as a function. There must not be spaces in these function calls. If the requested value is not found, an empty string is returned.
#### Get Header
`%[hdr(<header key>,<position>)]`
#### Get URL Parameter
`%[urlp(<parameter key>)]`
#### Get Metadata
`%[metadata(<metadata key>)]`
## Examples
See `header_processor_test.cc` for more examples.
```
// set-bool
http-request set-bool is_http %[hdr(x-forwarded-proto)] -m str http // let's assume this is true
http-request set-bool debug_hdr_exists %[hdr(debug)] -m found // let's assume this is true
http-request set-bool url_param_exists %[urlp(param)] -m found // let's assume this is true
http-request set-bool prefix_match example_string -m beg example // true
http-request set-bool false_bool example_string -m sub not_a_substring // false

// set-header
http-request set-header foo bar
http-request set-header header_key %[hdr(foo)] if is_http or debug_hdr_exists

// append-header
http-request append-header foo baz // will now be {"foo" : "bar,baz"}
http-request append-header long_header value1 value2 value3 //  {"long_header" : "value1,value2,value3"}

// set-path
http-request set-path newpath if not is_http

// set-metadata
http-request set-metadata metadata_key metadata_value if url_param_exists
http-request set-metadata metadata_key_copy %[metadata(metadata_key)] // should have the same value as above
```
## Extending the Filter
### Adding a New Dynamic Function
Adding a New Dynamic Function
#### Utilities

- Add the function name [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/utility.h#L47-L49) as a dynamic value, [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/utility.h#L75-L81) in the function type enum, and [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/utility.h#L86) to convert the function name to its enum value.

- If the function requires an argument, indicate that [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.cc#L606C39-L607C39) in the requiresArgument function.

#### Header Processor

- Add the function implementation [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.h#L42-L44) in the DynamicFunctionProcessor.

- If the function is only active on one path (eg. urlp should only be called on the request path), indicate that [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.cc#L606C39-L607C39) in parseOperation.

- Validate the number of arguments here for the function in the `DynamicFUnctionProcessor::parseOperation` switch statement [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.cc#L616-L636).

Call the function in `DynamicFunctionProcessor::executeOperation` [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.cc#L708-L729).
### Adding a New Operation
#### Utilities

Add the operation name [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/utility.h#L27-L31), [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/utility.h#L51-L58) in the operation type enum, and [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/utility.h#L83) to convert the function name to its enum value.

Add the minimum number of arguments for the operation [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/utility.h#L17-L23).

#### Header Processor

Based on the characteristics of the operation, derive the operation’s processor from the correct base class. If the operation requires a condition or needs to be parsed and executed in the Header Rewrite filter’s event loop (ie. for each incoming request and response), derive it from HeaderProcessor. If it’s a supporting operation that is parsed/executed based on another operation or at a different time, it can be derived from the base class Processor. Add the class declaration [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.h) in the header file and the implementation in header_processor.cc. At the very least, each operation must implement parseOperation and executeOperation.

#### Header Rewrite Filter

If the operation executes according to the Envoy request/response event loop, parse it at filter construction time [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_rewrite.cc#L60-L114). Otherwise, parse the operation whenever it needs to be parsed :smile: (An example is the ConditionProcessor, which happens [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_processor.cc#L11).) The same goes for executing the operation, which happens [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_rewrite.cc#L148) and/or [here](https://github.com/DataDog/envoy-header-rewrite/blob/main/header-rewrite-filter/header_rewrite.cc#L167) in decodeHeaders / encodeHeaders.
### Examples of What to Add
- Setting a variable

- Accessing metadata from another filter

- String utils such as trimming, getting the nth value from a list
## Impact and Potential
This project lays the groundwork for exploring upstream changes to Envoy. Through the Header Rewrite filter, we now have a fleshed out method for extending Envoy functionality.

More specifically with header processing, the Header Rewrite filter provides a theoretically more performant solution than the Lua filter, which incurs the overhead of running a Lua VM. Lastly, this project improves the developer experience associated with writing and maintaining code to perform header rewrites – the Header Rewrite filter’s simple but powerful DSL makes header processing much more painless.

## Additional Links
[Implementing Filters in Envoy](https://medium.com/@alishananda/implementing-filters-in-envoy-dcd8fc2d8fda)
