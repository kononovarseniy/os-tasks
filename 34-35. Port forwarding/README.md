Scheme
------
```
+------------------+     +--------+            +--------+     +---------+
| Service client A |<--->|        |            |        |<--->|         |
+------------------+     |        |   tunnel   |        |     |         |
                         | CLIENT |<==========>| SERVER |     | SERVICE | 
+------------------+     |        |            |        |     |         |
| Service client B |<--->|        |            |        |<--->|         |
+------------------+     +--------+            +--------+     +---------+
```
Build
-----
To build, just run make without arguments.

Run
-----
First start the server:
```
bin/portwfd server <server-tcp-port> <service-ip-address> <service-tcp-port>
```
Then client:
```
bin/portfwd client <tcp-port-for-incoming-connections> <server-ip-address> <server-tcp-port>
```
Now you can start service cients that connect to the client using ```<tcp-port-for-incoming-connections>```.

Example (all components are run on single host):
```
# Start echo server listening on tcp port 8080

bin/portfwd server 4040 127.0.0.1 8080
bin/portfwd client 2020 127.0.0.1 4040

# Connect to  with netcat
nc localhost 2020
```

Internal organization
---------------------
Client and server part of forwarder are almost symetrical.
Main difference between them is that client opens listening socket and accepts connections.

Connection manager (manager.c) manages all sockets.
It receive/send data to/from buffers, and presents information about new or closed connections.

Pump (pump.c) transfers data between buffers. It encode incoming data from buffers and put it to the tunnel buffer.
It sends commands and notify the controller about received commands.

Controller (controller.c) reacts to new connections and commands. It maintains collection of free connection identifiers.
