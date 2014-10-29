/*****************************************************************
 *
 * ZeroMQ as puredata external
 *
 * u@sansculotte.net 2014
 * version 0.0.2
 *
 */

#include "m_pd.h"
#include <string.h>
#include <stdlib.h>
#include <zmq.h>

#if (defined (WIN32))
# define randof(num) (int) ((float) (num) * rand () / (RAND_MAX + 1.0))
#else
# define randof(num) (int) ((float) (num) * random () / (RAND_MAX + 1.0))
#endif


// shim macros for version independence
#ifndef ZMQ_DONTWAIT
# define ZMQ_DONTWAIT ZMQ_NOBLOCK
#endif
#if ZMQ_VERSION_MAJOR == 2
# define zmq_msg_send(msg,sock,opt) zmq_send (sock, msg, opt)
# define zmq_msg_recv(msg,sock,opt) zmq_recv (sock, msg, opt)
# define zmq_ctx_destroy(context) zmq_term(context)
# define ZMQ_POLL_MSEC 1000 // zmq_poll is usec
# define ZMQ_SNDHWM ZMQ_HWM
# define ZMQ_RCVHWM ZMQ_HWM
#elif ZMQ_VERSION_MAJOR == 3
# define ZMQ_POLL_MSEC 1 // zmq_poll is msec
#endif


static t_class *zmq_class;

typedef struct _zmq {
   t_object  x_obj;
   void      *zmq_context;
   void      *zmq_socket;
   t_clock   *x_clock;
   t_outlet  *s_out;
   int       run_receiver;
//   int       socket_type;
} t_zmq;

void _zmq_error(int errno);
void _zmq_msg_tick(t_zmq *x);
void _zmq_close(t_zmq *x);
void _zmq_start_receiver(t_zmq *x);
void _zmq_stop_receiver(t_zmq *x);
static void _s_set_identity (t_zmq *x);

/**
 * constructor
 */
void* zmq_new(void)
{
   t_zmq *x = (t_zmq *)pd_new(zmq_class);

   char v[64];
   sprintf(v, "version: %i.%i.%i", ZMQ_VERSION_MAJOR, ZMQ_VERSION_MINOR, ZMQ_VERSION_PATCH);
   post(v);

#if ZMQ_VERSION_MAJOR > 2
   x->zmq_context = zmq_ctx_new();
#else
   x->zmq_context = zmq_init(1);
#endif
   if(x->zmq_context) {
      post("ØMQ context initialized");
      x->s_out = outlet_new(&x->x_obj, &s_symbol);
      x->x_clock = clock_new(x, (t_method)_zmq_msg_tick);
//      x->s_out = outlet_new(&x->x_obj, &s_float);
   } else {
      post("ØMQ context failed to initialize");
   }

   return (void *)x;
}

/**
 * destructor
 */
void zmq_destroy(t_zmq *x) {
   clock_free(x->x_clock);
   _zmq_stop_receiver(x);
   _zmq_close(x);
   if(x->zmq_context) {
#if ZMQ_VERSION_MAJOR > 2
      int r=zmq_ctx_destroy(x->zmq_context);
#else
      int r=zmq_term(x->zmq_context);
#endif
      if(r==0) post("ØMQ context destroyed");
   }
}

/**
 * ZMQ methods
 * http://api.zeromq.org
 */
void _zmq_about(t_zmq *x)
{
   post("ØMQ external u@sansculotte.net 2014\nhttp://api.zeromq.org");
}

/**
 * get ZMQ library version
 */
void _zmq_version(void) {
   int major, minor, patch;
   char verstr[64];
   zmq_version (&major, &minor, &patch);
   sprintf(verstr, "ØMQ version: %d.%d.%d\n", major, minor, patch);
   post(verstr);
}

/**
 * must match socket type
 * see: http://api.zeromq.org/3-2:zmq-socket
 */
void _zmq_create_socket(t_zmq *x, t_symbol *s) {
   // close any existing socket
   if(x->zmq_socket) {
       post("closing socket before openeing a new one");
       _zmq_close(x);
   }
   // push-pull/req-rep are the first pattern to implement for a POC
   int type = 0;
   if(strcmp(s->s_name, "push") == 0) {
      type = ZMQ_PUSH;
   } else
   if(strcmp(s->s_name, "pull") == 0) {
      type = ZMQ_PULL;
   } else
   if(strcmp(s->s_name, "request") == 0) {
      type = ZMQ_REQ;
   } else
   if(strcmp(s->s_name, "reply") == 0) {
      type = ZMQ_REP;
   } else
   if(strcmp(s->s_name, "publish") == 0) {
      type = ZMQ_PUB;
   } else
   if(strcmp(s->s_name, "subscribe") == 0) {
      type = ZMQ_SUB;
   }
   else {
      error("invalid socket type or not yet implemented");
      return;
   }

//   x->socket_type = type;
   x->zmq_socket = zmq_socket(x->zmq_context, type);
   _s_set_identity(x);

}

/**
 * start/stop the receiver loop
 */
void _zmq_start_receiver(t_zmq *x) {
    // do nothing when alread running
   if( x->run_receiver == 1) {
       return;
   }
   // check if the socket exists. there's no way in the api
   // to test if the socket is bound/connected
   if( ! x->zmq_socket) {
      error("create a socket first");
      return;
   }
   //int type = x->socket_type;
   int type;
   size_t len = sizeof (type);
   zmq_getsockopt(x->zmq_socket, ZMQ_TYPE, &type, &len);
   //   post("socket type: %d", type);
   // this needs to do more of these checks
   // most crashes seem to happen when starting the receive loop
   // also not sure if this is actually correct
   switch (type) {
       case ZMQ_REP:
       case ZMQ_REQ:
       case ZMQ_PULL:
       case ZMQ_SUB:
           post("starting receiver");
           x->run_receiver = 1;
           _zmq_msg_tick(x);
           break;
       default:
           post("socket type does not allow receive\n");
   }
}
void _zmq_stop_receiver(t_zmq *x) {
   x->run_receiver = 0;
}

/**
 * the receiver loop
 */
void _zmq_msg_tick(t_zmq *x) {

   if ( ! x->run_receiver) return;
   _zmq_receive(x);
   clock_delay(x->x_clock, 1);
}

/**
 * send an empty message
 * which will be received as bang
 * can be used for signalling/heartbeat
*/
void zmq_bang(t_zmq *x) {
   int r;
   if ( ! x->zmq_socket) {
      error("create and connect socket before sending anything");
      return;
   }
   r=zmq_send (x->zmq_socket, "", 0, ZMQ_DONTWAIT);
   if(r == -1) {
      _zmq_error(zmq_errno);
      return;
   }
//   outlet_float(x->s_out, rand());
}

/**
 * bind a socket
 * endpoint is a string consisting of <transport>://<address>
 * see: http://api.zeromq.org/3-2:zmq-bind
 */
void _zmq_bind(t_zmq *x, t_symbol *s) {
   if(! x->zmq_socket) {
      error("create a socket first");
      return;
   }
//   _zmq_close(x);
//   char* endpoint = &s->s_name;
   int r = zmq_bind(x->zmq_socket, s->s_name);
   if(r==0) {
      post("socket bound");
      // setup message listener
//      _zmq_msg_tick(x);
   }
   else _zmq_error(zmq_errno());
}
/**
 * unbind a socket rom the specified endpoint
 */
void _zmq_unbind(t_zmq *x, t_symbol *s) {
   if(! x->zmq_socket) {
      error("no socket");
      return;
   }
   int r = zmq_connect(x->zmq_socket, s->s_name);
   if(r==0) post("socket unbound");
   else _zmq_error(zmq_errno());
}

/**
 * create an outgoing connection, args are same as for bind
 */
void _zmq_connect(t_zmq *x, t_symbol *s) {
   if(! x->zmq_socket) {
      error("create socket first");
      return;
   }
//   _zmq_close(x);
//   char* endpoint = &s->s_name;
   int r = zmq_connect(x->zmq_socket, s->s_name);
   if(r==0) post("socket connected");
   else _zmq_error(zmq_errno());
}
/**
 * disconnect from specified endpoint
 */
void _zmq_disconnect(t_zmq *x, t_symbol *s) {
   if(! x->zmq_socket) {
      error("no socket");
      return;
   }
   int r = zmq_connect(x->zmq_socket, s->s_name);
   if(r==0) post("socket discconnected");
   else _zmq_error(zmq_errno());
}

/**
 * close socket
 */
void _zmq_close(t_zmq *x) {
   _zmq_stop_receiver(x);
   int r;
   if(x->zmq_socket) {
      int linger = 0;
      int len = sizeof (linger);
      zmq_setsockopt(x->zmq_socket, ZMQ_LINGER, &linger, &len);
      r=zmq_close(x->zmq_socket);
//      clock_unset(x->x_clock);
      if(r==0) {
         post("socket closed");
         //free(x->zmq_socket);
         x->zmq_socket = NULL;
      } else {
         _zmq_error(zmq_errno());
      }
   }
}

/*
void _zmq_send_float(t_zmq *x, t_float *t) {
   _zmq_send(x, gensym(t));
}
*/

/**
 * send a message
 */
void _zmq_send(t_zmq *x, t_symbol *s) {
   int r;
   char buf[MAXPDSTRING];
   int msg_len = strlen(s->s_name);
   if (msg_len==0) return;
   if ( ! x->zmq_socket) {
      error("create and connect socket before sending");
      return;
   }
   r=zmq_send (x->zmq_socket, s->s_name, msg_len, ZMQ_DONTWAIT);
   if(r == -1) {
      _zmq_error(zmq_errno);
      return;
   }
   // TODO:
   // for some socket types (esp REQ) this would better fetch the
   // reply in the same function than use the receieve loop
   return;
   r=zmq_recv (x->zmq_socket, buf, MAXPDSTRING, 0);
   buf[r] = 0; // terminate
   if(r>0) {
      outlet_symbol(x->s_out, gensym(buf));
   }
}

/**
 * fetch a message
 */
void _zmq_receive(t_zmq *x) {

   int r, err;
   char buf[MAXPDSTRING];

   r=zmq_recv (x->zmq_socket, buf, MAXPDSTRING, ZMQ_DONTWAIT);
   if(r != -1) {
       if (r > MAXPDSTRING) r = MAXPDSTRING; // brutally cut off excessive bytes
       buf[r] = 0; // terminate string

       if(r>0) {
          outlet_symbol(x->s_out, gensym(buf));
       } else {
          outlet_bang(x->s_out);
       }
       if((err=zmq_errno())!=EAGAIN) {
          _zmq_error(err);
       }
   }
}

/**
 * subscribe and unsubscribe for pub/sub pattern
 */
void _zmq_subscribe(t_zmq *x, t_symbol *s) {
   zmq_setsockopt(x->zmq_socket, ZMQ_SUBSCRIBE, s->s_name, strlen(s->s_name));
   post("subscribe to %s", s->s_name);
   _zmq_start_receiver(x);
}
void _zmq_unsubscribe(t_zmq *x, t_symbol *s) {
   zmq_setsockopt(x, ZMQ_UNSUBSCRIBE, s, strlen(s));
   post("unsubscribe from %s", s->s_name);
   _zmq_stop_receiver(x);
}

/**
 * create object
 */
void zmq_setup(void)
{
   zmq_class = class_new(gensym("zmq"),
         (t_newmethod)zmq_new,
         (t_method)zmq_destroy,
         sizeof(t_zmq),
         CLASS_DEFAULT, A_GIMME, 0);

   class_addbang(zmq_class, zmq_bang);
   class_addmethod(zmq_class, (t_method)_zmq_about, gensym("about"), 0);
   class_addmethod(zmq_class, (t_method)_zmq_version, gensym("version"), 0);
   class_addmethod(zmq_class, (t_method)_zmq_create_socket, gensym("socket"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_bind, gensym("bind"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_unbind, gensym("unbind"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_connect, gensym("connect"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_disconnect, gensym("disconnect"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_close, gensym("close"), 0);
   class_addmethod(zmq_class, (t_method)_zmq_receive, gensym("receive"), 0);
   class_addmethod(zmq_class, (t_method)_zmq_start_receiver, gensym("start_receive"), 0);
   class_addmethod(zmq_class, (t_method)_zmq_stop_receiver, gensym("stop_receive"), 0);
   class_addmethod(zmq_class, (t_method)_zmq_send, gensym("send"), A_SYMBOL, 0);
//   class_addmethod(zmq_class, (t_method)_zmq_send_float, gensym("send"), A_FLOAT, 0);
   class_addmethod(zmq_class, (t_method)_zmq_subscribe, gensym("subscribe"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_unsubscribe, gensym("unsubscribe"), A_SYMBOL, 0);
}

/**
 * from zhelpers.h
 *
 * Set simple random printable identity on socket
*/
static void _s_set_identity (t_zmq *x) {
    char identity [10];
    sprintf (identity, "%04X-%04X", randof (0x10000), randof (0x10000));
    zmq_setsockopt (x->zmq_socket, ZMQ_IDENTITY, identity, strlen (identity));
    post("socket identity: %s", identity);
}

/**
 * error translator
 */
void _zmq_error(int errno) {
   error(zmq_strerror(errno));
   /*
   switch(errno) {
      case EINVAL:
         error("The endpoint supplied is invalid."); break;
      case EPROTONOSUPPORT:
         error("The requested transport protocol is not supported."); break;
      case ENOCOMPATPROTO:
         error("The requested transport protocol is not compatible with the socket type."); break;
      case EADDRINUSE:
         error("The requested address is already in use."); break;
      case EADDRNOTAVAIL:
         error("The requested address was not local."); break;
      case ENOTSOCK:
         error("The provided socket was invalid."); break;
      default:
         error("error not caught or all good");
   }
   */
}
