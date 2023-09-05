# Building and Running the Header Rewrite Filter

## Clone the repo
`$ git clone git@github.com:DataDog/envoy-header-rewrite.git`
## Update the Envoy submodule and build the plugin
```
$ git submodule update --init
$ bazel build //header-rewrite-filter:envoy
Starting local Bazel server and connecting to it...

INFO: Analyzed target //header-rewrite-filter:envoy (953 packages loaded, 44450 targets configured).

INFO: Found 1 target...

Target //header-rewrite-filter:envoy up-to-date:

  bazel-bin/header-rewrite-filter/envoy

INFO: Elapsed time: 23.088s, Critical Path: 14.01s

INFO: 4 processes: 3 internal, 1 linux-sandbox.

INFO: Build completed successfully, 4 total actions
```

## Run Envoy
```
$ ./bazel-bin/header-rewrite-filter/envoy -c ./header-rewrite-filter/envoy-sample-config.yaml

[2023-06-30 14:40:11.458][151981][info][main] [external/envoy/source/server/server.cc:412] initializing epoch 0 (base id=0, hot restart version=11.104)

[2023-06-30 14:40:11.458][151981][info][main] [external/envoy/source/server/server.cc:415] statically linked extensions:

[2023-06-30 14:40:11.458][151981][info][main] [external/envoy/source/server/server.cc:417]   envoy.thrift_proxy.transports: auto, framed, header, unframed

…

[2023-06-30 14:40:11.572][151981][info][upstream] [external/envoy/source/common/upstream/cluster_manager_impl.cc:226] cm init: all clusters initialized

[2023-06-30 14:40:11.572][151981][warning][main] [external/envoy/source/server/server.cc:811] there is no configured limit to the number of allowed active connections. Set a limit via the runtime key overload.global_downstream_max_connections

[2023-06-30 14:40:11.573][151981][info][main] [external/envoy/source/server/server.cc:918] all clusters initialized. initializing init manager

[2023-06-30 14:40:11.573][151981][info][config] [external/envoy/source/extensions/listener_managers/listener_manager/listener_manager_impl.cc:857] all dependencies initialized. starting workers

[2023-06-30 14:40:11.605][151981][info][main] [external/envoy/source/server/server.cc:937] starting main dispatch loop
```
## Verify Filter Plugin Behavior
### Set Up Echo Server
The echo server will echo the HTTP headers of each request that it receives. We will set this up as a dummy backend to verify Envoy’s behavior.
```
$ docker pull ealen/echo-server
$ docker run -d -p 3000:80 ealen/echo-server
```
You can also set up an echo server directly on the command line as follows:
```
$ while true; do printf 'HTTP/1.1 200 OK\r\n\r\n' | nc -Nl 8000; done
```

### Note About Entrypoints
There are 2 ports of interest: the Envoy entry point and the port that your echo server is listening on.

The port number associated with the Envoy listener filter is the entry point and should be the target of a `curl`. It should look like this in the Envoy config:
```
static_resources:
  listeners:
  - name: web
    address:
      socket_address:
        protocol: TCP
        address: 0.0.0.0
        port_value: 8081 # envoy entrypoint
```
The port that the echo server is listening on should match the port that Envoy forwards requests to. It should be at the end of the clusters section in the Envoy config:
```
clusters:
  - name: local_service
    connect_timeout: 30s
    type: STRICT_DNS
    lb_policy: ROUND_ROBIN
    upstream_connection_options:
      tcp_keepalive:
        keepalive_time: 300
    common_lb_config:
      healthy_panic_threshold:
        value: 40
    load_assignment:
      cluster_name: local_service
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: 8000 # connected to echo server
```
### Observe Filter Behavior
```
$ curl -v localhost:8081
```

## Unit Tests
`bazel test --test_output=all //header-rewrite-filter:header_processor_test`

`bazel test --test_output=all //header-rewrite-filter:header_rewrite_integration_test`
