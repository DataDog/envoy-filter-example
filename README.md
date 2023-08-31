# Envoy Header Rewrite Filter Documentation
## Introduction
### Envoy and HTTP Filters
One of Envoy’s very useful features is its ability to hook into the L7 network layer using HTTP filters. HTTP filters are essentially Envoy modules that can be added to the request and/or response path to carry out a network behavior. HTTP filters operate on HTTP messages without knowledge of the underlying protocol, making it easy to inspect and modify state at this network layer. HTTP filters are also modular and ordered – they can programmatically be enabled and are invoked in a specific order along a path – which is very convenient for mixing and matching different filters to achieve the desired network behaviors. Here is a list of all the HTTP filters that Envoy has developed.

There are three types of HTTP filters: decoder filters (active on the request path), encoder filters (active on the response path), decoder/encoder filters (active on both paths). Below is an example HTTP filter chain illustrating which filters are activated along each path and in which order they are activated:

 ![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/a06f4023-aa56-4db7-86e9-0079ab71553a)
 
From the Envoy docs: “HTTP filters can stop and continue iteration to subsequent filters. This allows for more complex scenarios such as health check handling, calling a rate limiting service, buffering, routing, generating statistics for application traffic such as DynamoDB, etc. HTTP level filters can also share state (static and dynamic) among themselves within the context of a single request stream. Refer to data sharing between filters for more details.”

## Motivation
This filter improves the developer experience and performance of performing HTTP header rewrites.
- The Lua filter, an HTTP filter that can also perform header rewrites, requires writing lots of code to perform even very simple header rewrites.
- The Lua filter invalidates the route cache when request headers are modified, which has a performance impact that can be avoided.
- The Lua filter’s functionality is limited to the APIs that are implemented within the filter. While there are currently a good amount of Envoy wrappers exposed within the Lua filter, there are still more that have not yet been implemented, such as information about retransmits, bytes received, etc. A custom filter is thus more extensible, as any desired functionality can be added at any time.
## Implementation
The Header Rewrite Filter executes as part of the HTTP filter chain managed by the HTTP Connection Manager, which is a network filter. It follows a parse-execute structure.

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/19832af7-c763-47b4-a8a1-2fe98524c98b)

### Parse
The Header Rewrite filter protobuf struct looks like this:
```
message HeaderRewrite {
  string config = 1 [(validate.rules).string.min_len = 1];
}
```
At initialization time, the filter takes a single string value as its configuration. This string value should contain a list of header rewrite operations with each operation separated by a newline. When parsing the config, the filter first splits the string by newline (i.e. by operation) and parses each operation one at a time. For each operation, the filter will construct a Processor object that carries out the parse-execute sequence for that operation. The input to the Processor’s parse function is a vector of string_view's, which is simply the operation split by spaces. (Spaces are thus special characters and should not be used unless specified).

At this step, a Processor will do the following:

- Check for syntax errors and other errors (such as a nonexistent boolean variable being referenced in the operation)

- Check for a condition within the operation. If a condition is found (designated by the keyword if), a Processor will initialize a ConditionProcessor and direct it to parse the conditional expression following the if.

- Store all relevant information such as arguments to the operation, arguments to dynamic functions being called within the function, operators/operands from the conditional expression

The Header Rewrite filter keeps a vector of request-side Processors and response-side Processors. It checks the value of the first token in each operation, which should be either http-request or http-response, to determine what vector to append the Processor to. It will append the Processor to the proper vector after directing that Processor to parse its operation. (Note: BoolProcessors are stored separately, as they are not executing in the main request/response path and are instead invoked each time a boolean variable is referenced in an operation’s condition).

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/26004bbf-3f51-4011-894d-fc1bbf84faf9)

### Execute
When a request or a response comes in, the Header Rewrite filter’s specialized callback is triggered. On the request path, decodeHeaders is called. On the response path, encodeHeaders is called. In the triggered callback, the Header Rewrite filter will go through the list of Processors and direct each one to execute its respective operation.

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/3109e0de-c716-40c2-bf11-49f7a2716799)

### Processor Class Structure
All processors are derived from the Processor base class, which contains the base functionality common to all processors (eg. parsing, whether the operation is request or response side). A HeaderProcessor is a second base class which implements hooking a conditional expression, supported by a ConditionProcessor, into an operation (this is probably not the best name for the base class and would be better named as ProcessorWithCondition or something similar).

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/fe70ac72-9779-4e36-a7e1-42ab4ae76a6c)

### Conditions
A ConditionProcessor is a member of HeaderProcessor. When parsing a header operation, if the if keyword is used a condition is detected. HeaderProcessor will set up and parse the ConditionProcessor. The condition is evaluated in each HeaderProcessor’s executeOperation. Conditions are made of boolean variables, which are declared as set-bool operations in the filter's config. Each boolean operation is processed in its own SetBoolProcessor. When a condition is being evaluated, it must look up the value of a boolean variable. It uses the bool_processors_ map (which is a member variable of all Processor's) to look up the SetBoolProcessor for that boolean variable and calls the SetBoolProcessor’s executeOperation. Thus, dynamic values such as header values, URL parameters, and metadata are fetched at execution time and are consistent with the latest header rewrite operations that have been applied to the request/response.

![image](https://github.com/DataDog/envoy-header-rewrite/assets/66568876/9bd957c3-7eda-403c-97dd-33db433d25ad)

## DSL
A header rewrite command follows this general structure:

<active path> <operation type> <arguments> if <conditional expression>

Things to keep in mind:

- Because the filter’s parse step will split each operation by a space character, spaces are special characters and must not be used anywhere other than the positions specified in the syntax for each operation. Extra spaces in the positions specified are tolerated but should be avoided.

- if is a keyword and thus cannot be used as a string literal.

- Numbers are treated like strings.
### Operations

#### Set Header
<http-request/http-response> set-header <key> <value> if <conditional expression>

- key or value can be a static string literal or the result of a dynamic function (see related section below)

- if key does not exist a new header will be created, otherwise if a header already exists with the given key, its value will be replaced with value
#### Append Header
#### Set Path
#### Set Metadata
### Conditional Expressions
#### Set Bool
### Match Types
#### Exact
#### Prefix
#### Substring
#### Found
### Dynamic Functions
#### Get Header
#### Get URL Parameter
#### Get Metadata
## Examples
## Extending the Filter
### Adding a New Dynamic Function
### Adding a New Operation
### Examples of What to Add
## Impact and Potential
## Additional Links
