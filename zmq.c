/////////////////////////////////////////////////////////////////////////////
// 
// zeroMQ bindings for puredata
//
// u@sansculotte.net 2014
//

#include "m_pd.h"
#include <string.h>
#include <stdlib.h>
#include <zmq.h>

static t_class *zmq_class;

typedef struct _zmq {
   t_object  x_obj;
   void     *zmq_context;
   void     *zmq_socket;
   t_clock  *x_clock;
   t_outlet *s_out;
} t_zmq;

void _zmq_error(int errno);
void _zmq_msg_tick(t_zmq *x); 
void _zmq_close(t_zmq *x); 

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

void zmq_free(t_zmq *x) {
   clock_free(x->x_clock);
}

void zmq_destroy(t_zmq *x) {
   if(x->zmq_socket) {
      zmq_close(x->zmq_socket);
   }
   if(x->zmq_context) {
#if ZMQ_VERSION_MAJOR > 2
      zmq_ctx_destroy(x->zmq_context);
#else
      zmq_term(x->zmq_context);
#endif
      post("ØMQ context destroyed");
   }
}

// ZMQ methods
// http://api.zeromq.org
void _zmq_about(t_zmq *x)
{
   post("ØMQ external u@sansculotte.net 2014\nhttp://api.zeromq.org");
}

void _zmq_version(void) {
   int major, minor, patch;
   char verstr[64];
   zmq_version (&major, &minor, &patch);
   sprintf(verstr, "ØMQ version: %d.%d.%d\n", major, minor, patch);
   post(verstr);
}

// must match socket type
// see: http://api.zeromq.org/3-2:zmq-socket
void _zmq_create_socket(t_zmq *x, t_symbol *s) {
   // close any existing socket
   _zmq_close(x);
   // push-pull/req-rep are the first pattern to implement for a POC
   int type;
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

   x->zmq_socket = zmq_socket(x->zmq_context, type);
}

void _zmq_msg_tick(t_zmq *x) {
   // output messages shere later
   int r, err;
   char buf[MAXPDSTRING];
//   int rnd;
//   sprintf (buf, "%i", rand()%23);
   r=zmq_recv (x->zmq_socket, buf, MAXPDSTRING, ZMQ_DONTWAIT);
   buf[r] = 0;
   if(r>0) {
      outlet_symbol(x->s_out, gensym(buf));
   }
   if((err=zmq_errno())!=EAGAIN) {
      _zmq_error(err);
   }
//   outlet_symbol(x->s_out, gensym(buf));
   clock_delay(x->x_clock, 1);
}

// test tick function
void zmq_bang(t_zmq *x) {
   _zmq_msg_tick(x);
//   outlet_float(x->s_out, rand());
}

// binds a listening socket
// The endpoint is a string consisting of a transport :// followed by an address 
// see: http://api.zeromq.org/3-2:zmq-bind
void _zmq_bind(t_zmq *x, t_symbol *s) {
   if(! x->zmq_socket) {
      error("create a socket first");
      return;
   } 
//   _zmq_close(x);
//   char* endpoint = &s->s_name;
   int r = zmq_bind(x->zmq_socket, s->s_name);
   if(r==0) {
      post("socket bound\n");
      // setup message listener
      _zmq_msg_tick(x);
   }
   else _zmq_error(zmq_errno());
}

// create an outgoing connection, args are same as for bind
void _zmq_connect(t_zmq *x, t_symbol *s) {
   if(! x->zmq_socket) {
      error("create socket first");
      return;
   }
//   _zmq_close(x);
//   char* endpoint = &s->s_name;
   int r = zmq_connect(x->zmq_socket, s->s_name);
   if(r==0) post("socket connected\n");
   else _zmq_error(zmq_errno());
}

// close socket
void _zmq_close(t_zmq *x) {
   int r;
   if(x->zmq_socket) {
      r=zmq_close(x->zmq_socket);
      clock_unset(x->x_clock);
      if(r==0) {
         post("socket closed");
      } else {
         _zmq_error(zmq_errno());
      }
   }
}

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
   // do nonblocking mode here or listen for messages in a different loop
   r=zmq_recv (x->zmq_socket, buf, MAXPDSTRING, 0);
   if(r>0) {
      outlet_symbol(x->s_out, gensym(buf));
   }
}

// subscribe and unsubscribe for pub/sub pattern
void _zmq_subscribe(t_zmq *x, t_symbol *s) {
   zmq_setsockopt(x->zmq_socket, ZMQ_SUBSCRIBE, s->s_name, strlen(s->s_name));
   post("subscribe to %s", s->s_name);
   _zmq_msg_tick(x);
}

void _zmq_unsubscribe(t_zmq *x, t_symbol *s) {
   zmq_setsockopt(x, ZMQ_UNSUBSCRIBE, s, strlen(s));
   post("unsubscribe from %s", s->s_name);
}

// create object
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
   class_addmethod(zmq_class, (t_method)_zmq_connect, gensym("connect"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_close, gensym("close"), 0);
   class_addmethod(zmq_class, (t_method)_zmq_send, gensym("send"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_subscribe, gensym("subscribe"), A_SYMBOL, 0);
   class_addmethod(zmq_class, (t_method)_zmq_unsubscribe, gensym("unsubscribe"), A_SYMBOL, 0);
}

// error translator
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


