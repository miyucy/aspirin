#include "aspirin.h"
static void  aspirin_server_mark(Aspirin_Server*);
static void  aspirin_server_free(Aspirin_Server*);
static void  aspirin_server_base_initialize(Aspirin_Server*);
static void  aspirin_server_signal_initialize(Aspirin_Server*);
static void  aspirin_server_stop(int, short, void*);
static void  aspirin_server_stop_bang(int, short, void*);
static void  aspirin_server_http_initialize(Aspirin_Server*);
static VALUE aspirin_server_host(VALUE);
static int   aspirin_server_port(VALUE);
static void  aspirin_server_http_request(struct evhttp_request*, void*);

VALUE
aspirin_server_alloc(VALUE klass)
{
    return Data_Wrap_Struct(klass, aspirin_server_mark, aspirin_server_free, 0);
}

static void
aspirin_server_mark(Aspirin_Server* srv)
{
    if(!srv)
    {
        return;
    }
    rb_gc_mark(srv->app);
    rb_gc_mark(srv->options);
    rb_gc_mark(srv->env);
}

static void
aspirin_server_free(Aspirin_Server* srv)
{
    if(!srv)
    {
        return;
    }

    event_del(&srv->sig_int);
    event_del(&srv->sig_quit);
    event_del(&srv->sig_term);

    if(srv->http)
    {
        evhttp_free(srv->http);
        srv->http = NULL;
    }

    if(srv->base)
    {
        event_base_loopbreak(srv->base);
        event_base_free(srv->base);
        srv->base = NULL;
    }

    xfree(srv);
}

VALUE
aspirin_server_initialize(VALUE obj, VALUE app, VALUE options)
{
    Aspirin_Server *srv;

    Data_Get_Struct(obj, Aspirin_Server, srv);
    if(!srv)
    {
        srv = ALLOC(Aspirin_Server);
        MEMZERO(srv, Aspirin_Server, 1);
        DATA_PTR(obj) = srv;
    }

    srv->app = app;
    srv->options = options;
    srv->env = dupe_default_env();
    srv->exit = 0;

    aspirin_server_base_initialize(srv);
    aspirin_server_signal_initialize(srv);
    aspirin_server_http_initialize(srv);

    return obj;
}

static void
aspirin_server_base_initialize(Aspirin_Server *srv)
{
    srv->base = event_base_new();
}

static void
aspirin_server_signal_initialize(Aspirin_Server *srv)
{
    event_set(&srv->sig_quit, SIGQUIT, EV_SIGNAL|EV_PERSIST, aspirin_server_stop, srv);
    event_base_set(srv->base, &srv->sig_quit);
    event_add(&srv->sig_quit, NULL);

    event_set(&srv->sig_int,  SIGINT, EV_SIGNAL|EV_PERSIST, aspirin_server_stop_bang, srv);
    event_base_set(srv->base, &srv->sig_int);
    event_add(&srv->sig_int, NULL);

    event_set(&srv->sig_term, SIGTERM, EV_SIGNAL|EV_PERSIST, aspirin_server_stop_bang, srv);
    event_base_set(srv->base, &srv->sig_term);
    event_add(&srv->sig_term, NULL);
}

static void
aspirin_server_stop(int fd, short event, void *arg)
{
    Aspirin_Server *srv = arg;
    struct timeval delay = {1, 0};
    srv->exit = 1;
    event_del(&srv->sig_quit);
    event_base_loopexit(srv->base, &delay);
}

static void
aspirin_server_stop_bang(int fd, short event, void *arg)
{
    Aspirin_Server *srv = arg;
    srv->exit = 1;
    event_base_loopbreak(srv->base);
}

static void
aspirin_server_http_initialize(Aspirin_Server *srv)
{
    VALUE host = aspirin_server_host(srv->options);
    int   port = aspirin_server_port(srv->options);

    StringValue(host);
    rb_hash_aset(srv->env, global_envs[GE_SERVER_NAME], rb_obj_freeze(host));

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    rb_hash_aset(srv->env, global_envs[GE_SERVER_PORT], FRZSTR(port_str));

    srv->http = evhttp_new(srv->base);
    evhttp_bind_socket(srv->http, RSTRING_PTR(host), port);
    evhttp_set_gencb(srv->http, aspirin_server_http_request, srv);
}

static VALUE
aspirin_server_host(VALUE options)
{
    VALUE address = Qnil;
    if(TYPE(options) == T_HASH)
    {
        address = rb_hash_aref(options, ID2SYM(rb_intern("Host")));
        if(!NIL_P(address)){
            return address;
        }
    }
    return rb_str_new2("0.0.0.0");
}

static int
aspirin_server_port(VALUE options)
{
    VALUE port = Qnil;
    if(TYPE(options) == T_HASH)
    {
        port = rb_hash_aref(options, ID2SYM(rb_intern("Port")));
        if(!NIL_P(port)){
            return NUM2INT(port);
        }
    }
    return 9292;
}

static void
aspirin_server_http_request(struct evhttp_request *req, void *arg)
{
    Aspirin_Server *srv = arg;
    aspirin_response_start(req, srv->app, srv->env);
}

static VALUE
aspirin_server_thread(Aspirin_Server* srv)
{
    while(event_base_loop(srv->base, EVLOOP_NONBLOCK) != 1 && srv->exit == 0)
    {
        rb_thread_schedule();
    }
    return Qnil;
}

VALUE
aspirin_server_start(VALUE obj)
{
    VALUE thread = rb_thread_create(aspirin_server_thread,
                                    (void*)DATA_PTR(obj));
    return rb_funcall(thread, rb_intern("join"), 0);
}

VALUE
aspirin_server_shutdown(VALUE obj)
{
    Aspirin_Server *srv = DATA_PTR(obj);
    event_base_loopbreak(srv->base);
    return Qnil;
}
