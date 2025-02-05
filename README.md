# x64dbg Debug Automation Powered by ZMQ

This repository contains the source code for the RPC server of x64dbg Automate. It is used by the x64dbg Automate [Python Client](https://github.com/dariushoule/x64dbg-automate-pyclient) to broker IPC between x64dbg and client automation software.

The RPC Server is built on [ZeroMQ](https://zeromq.org/) and supports concurrent debug control and event notification. Message seriailization is performed using [messagepack](https://msgpack.org/index.html).

## Documentation

Full project documentation is published on: [https://dariushoule.github.io/x64dbg-automate-pyclient/](https://dariushoule.github.io/x64dbg-automate-pyclient/)

See: [Installation](https://dariushoule.github.io/x64dbg-automate-pyclient/installation/) and [Quickstart](https://dariushoule.github.io/x64dbg-automate-pyclient/quickstart/)

🔔 _All examples and sample code assume x64dbg is configured to stop on entry and system breakpoints, skipping TLS breakpoints._

## Extending

Building new features into the RPC interface is easy.

### For new request/reply style calls

See: [x64dbg-automate/blob/main/src/xauto_server.cpp#L27](https://github.com/dariushoule/x64dbg-automate/blob/main/src/xauto_server.cpp#L27)

RPC requests consist of a command identifier and then an argument tuple. Adding new calls is as simple as creating a new command identifier, parsing your argument tuple, and serializing a response.

See the implementation of `XAUTO_REQ_DBG_EVAL` as an example:
[x64dbg-automate/blob/main/src/xauto_cmd.cpp#L20](https://github.com/dariushoule/x64dbg-automate/blob/main/src/xauto_cmd.cpp#L20)

This is in turn used by the Python client as so:
[x64dbg-automate-pyclient/blob/main/x64dbg_automate/commands_xauto.py#L64](https://github.com/dariushoule/x64dbg-automate-pyclient/blob/main/x64dbg_automate/commands_xauto.py#L64)

### For new pub/sub style events

Events are generally published during plugin callback lifecycle events. Registered callbacks can publish data to the pub/sub ZMQ socket at will and the python client will receive and queue the events.

See: [x64dbg-automate/blob/main/src/plugin.cpp#L143](https://github.com/dariushoule/x64dbg-automate/blob/main/src/plugin.cpp#L143)

Events are processed and stored by the Python client here:
[x64dbg-automate-pyclient/blob/main/x64dbg_automate/events.py#L85](https://github.com/dariushoule/x64dbg-automate-pyclient/blob/main/x64dbg_automate/events.py#L85)

## Building

From a Visual Studio command prompt:

```
cmake -B build64 -A x64
cmake --build build64 --config Release
```

To build the 32-bit plugin:

```
cmake -B build32 -A Win32
cmake --build build32 --config Release
```

# Contributing

Issues, feature-requests, and pull-requests are welcome on this project ❤️🐛

My commitment to the community will be to be a responsive maintainer. Discuss with me before implementing major breaking changes or feature additions.