/////////////////////////////////////////////////////////////////////////////
// 
// zeroMQ bindings for puredata
//
// u@sansculotte.net 2014
//

#include "m_pd.h"
#include <zmq.h>

static t_class *zmq_class;

typedef struct _zmq {
   t_object  x_obj;
   void* zmq_context;
} t_zmq;

void* zmq_new(void)
{
   t_zmq *x = (t_zmq *)pd_new(zmq_class);

#if ZMQ_VERSION_MAJOR > 3
   x->zmq_context = zmq_ctx_new();
#else
   x->zmq_context = zmq_init(1);
#endif
   if(x->zmq_context) {
      post("ØMQ context initialized");
   } else {
      post("ØMQ context failed to initialize");
   }

   return (void *)x;
}

void zmq_destroy(t_zmq *x) {
   if(x->zmq_context) {
#if ZMQ_VERSION_MAJOR > 3
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
   post("ØMQ external u@sansculotte.net 2014\nhttp://api.zeromq.org\n");
}

void _zmq_version(void) {
   int major, minor, patch;
   char verstr[64];
   zmq_version (&major, &minor, &patch);
   sprintf(verstr, "ØMQ version: %d.%d.%d\n", major, minor, patch);
   post(verstr);
}

// create object
void zmq_setup(void)
{
   zmq_class = class_new(gensym("zmq"),
         (t_newmethod)zmq_new,
         (t_method)zmq_destroy,
         sizeof(t_zmq),
         CLASS_DEFAULT, A_GIMME, 0);

   //     class_addbang(zmq_class, zmq_bang);
   class_addmethod(zmq_class, (t_method)_zmq_about, gensym("about"), 0);
   class_addmethod(zmq_class, (t_method)_zmq_version, gensym("version"), 0);
}
