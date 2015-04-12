ØMQ external for puredata
=========================

ZeroMQ is a barebone message Queue system with variable architecture, see:
http://zeromq.org
Puredata is a visual multimedia programming environment, see:
http://puredata.info

version 0.0.3
at the moment functionality is basic, but the thing usable.

send and receive are using the FUDI protocol, which makes it great for
connecting PD patches, but slightly akward for use with external nodes.


what it can do:
* create PUSH/PULL, REQ/REPL and PUB/SUB sockets
* connect/bind clients to socket
* send and receive pd FUDI messages analogous to netsend/netreceive
* some error checking to prevent the worst foot-shooting

what's planned:
see TODO



Installation
============

You need to have libzmq installed. On debian based systems:
apt-get install libzmq-dev

cd into your pd sourcetrees external folder.

    git clone git@github.com:sansculotte/pd-zmq.git
    make

this creates the external zmq.pd_linux



Known Bugs
==========

* crashes when binding to port already in use by another zmq agent
* crashes when starting receiver on REPL socket
