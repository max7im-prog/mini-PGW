# Mini PGW

A small emulator of a PGW (Package GateWay) with following capabilities:

- Handling UDP requests with IMSI in a BCD format
- Controlling sessions by IMSI
- Logging of CDR (Call Detail Records)
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
- /docs - documentation

## Build instructions

The project is written in C++ and built with CMake and therefore follows the basic build process:

```bash
git clone https://github.com/max7im-prog/mini-PGW.git
mkdir build
cd build
cmake .. 
cmake --build .
```


## Running a program

Upon building a project, 2 executables are created in the build directory: **pgw_server** and **pgw_client**. The server can be run with:

```bash
pgw_server <path to server config>
```

Upon starting, the server will begin processing UDP packets sent to UDP port and processing HTTP connections on HTTP port.

The client can be run with:

```bash
pgw_server <path to client config> <IMSI>
```

Upon starting, the client will send an IMSI in BCD format to the server mentioned in a configuration file and then wait for the response.

## Configuration

Both server and client use configuration files. Their structure is as follows:

Server:

```json
{
  "numUdpThreads": 5,
  "ip": "127.0.0.1",
  "udpPort": 8080,
  "httpPort": 9000,
  "sessionTimeoutSec": 30,
  "maxShutdownTimeSec": 10,
  "preferredShutdownRateSessionsPerSec":100,
  "cdrFileName": "logs/cdr.log",
  "logFileName": "logs/server.log",
  "logLevel": "DEBUG",
  "blacklist": [
    "111"
  ]
}
```

Client:

```json
{
  "serverIp": "127.0.0.1",
  "serverPort": 8080,
  "hasTimeout": true,
  "clientTimeoutSec": 5,
  "logFileName": "logs/client.log",
  "logLevel": "INFO",
  "quiet": false
}
```

## HTTP 

Server provides several HTTP endpoints:

- /stop - query this to stop the server and trigger gracefull offload
- /check_subscriber?imsi=IMSI - checks if the subscriber with IMSI has an active session corresponding to him


## Documentation

Project has a doxygen configured. To generate documentation, go to the project's root and use:

```bash
doxygen ./docs/Doxyfile 
```

That will generate a documentation in **/docs/html** directory of a project
