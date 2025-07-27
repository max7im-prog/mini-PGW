# Mini PGW

A small emulator of a PGW (Package GateWay) with following capabilities:

- Handling UDP requests with IMSI in a BCD format
- Controlling sessions by IMSI
- Logging of CDR (Call Detail Records) events
- Handling HTTP requests
- Support of an IMSI blacklist 
- Graceful shutdown

This is, however, only an emulator and it does not support packet forwarding, ARP resolution, etc.

## Project structure

- /src/client - source code of a client
- /src/server - source code of a server
- /src/common - code used by both server and client
- /test - testing
- /res - resources

 ## Build instructions

 The program is written in C++ and built with CMake therefore it follows the basic build process:

```bash

```


