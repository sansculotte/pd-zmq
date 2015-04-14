ØMQ external for puredata
=========================

ZeroMQ is a barebone, ultra-efficient message Queue system with a flexible
architecture, see:
http://zeromq.org
Puredata is a visual multimedia programming environment, see:
http://puredata.info

version 0.0.3
at the moment functionality is basic, but the thing is generally usable.
zeromq is a very basic but powerfull library so it is easy to shoot your-
self in the foot. check out the "FAQs":http://zeromq.org/area:faq for
some advice on usage, or consult "the guide":http://zguide.zeromq.org/page:all

send and receive are using the FUDI protocol, which makes it great for
connecting PD patches, but slightly akward for use with external nodes.


what it can do:
* create PUSH/PULL, REQ/REPL and PUB/SUB sockets
* connect/bind clients to socket
* send and receive pd FUDI messages analogous to netsend/netreceive
* some error checking to prevent the worst foot-shooting

what's planned:
see the TODO file

found a bug?
report it to https://github.com/sansculotte/pd-zmq/issues



Installation
============

You need to have libzmq installed. On debian based systems:
apt-get install libzmq-dev

cd into your pd sourcetrees external folder.

    git clone git@github.com:sansculotte/pd-zmq.git
    make

builds the external zmq.pd_linux which you can then put in pd's path,
either by moving it there or setting the path accordingly.



Known Bugs
==========

* crashes when binding to port already in use by another zmq agent
* crashes when starting receiver on REPL socket
* crashes when sending on REPL socket without prior REQUEST
* crashes when sending on a BOUND socket with no peers
    appearently there is no queue alocated to store the message.
    when you connect, disconnect before bind it actually works, but
    i'm not sure if that qualifies as a workaround.
    see: http://zeromq.org/area:faq
    Why do I see different behavior when I bind a socket versus connect a socket?
