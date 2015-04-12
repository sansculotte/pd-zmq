ØMQ external for puredata
=========================

ZeroMQ is a barebone message Queue system with variable architecture, see:
http://zeromq.org
Puredata is a visual multimedia programming environment, see:
http://puredata.info

version 0.0.3
at the moment functionality is basic, but usable. 

what it can do:
* create PUSH/PULL, REQ/REPL and PUB/SUB sockets
* connect/bind clients to socket
* send and receive pd FUDI messages analogous to netsend/netreceive
* some error checking to prevent the worst foot-shooting
