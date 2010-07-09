#ifndef __ASPIRIN_H__
#define __ASPIRIN_H__

#include <ruby.h>
#ifdef HAVE_RUBY_ST_H
#include <ruby/st.h>
#else
#include <st.h>
#endif

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <sys/queue.h>

#include <event.h>
#include <evhttp.h>
#include <evutil.h>

#define INSPECT(obj) {VALUE __obj__ = rb_inspect((obj)); fprintf(stderr, "%d:%s\n", __LINE__, RSTRING_PTR(__obj__));}
#define FRZSTR(str) (rb_obj_freeze(rb_str_new2((str))))

enum
{
    GE_GET,
    GE_HEAD,
    GE_HTTP_VERSION,
    GE_PATH_INFO,
    GE_POST,
    GE_QUERY_STRING,
    GE_RACK_ERRORS,
    GE_RACK_INPUT,
    GE_REMOTE_ADDR,
    GE_REQUEST_METHOD,
    GE_REQUEST_PATH,
    GE_REQUEST_URI,
    GE_SERVER_NAME,
    GE_SERVER_PORT,
    GE_FRAGMENT,
    GE_EMPTY,
    GE_ASYNC_CALLBACK,
    GE_CALL,
    GE_ASYNC_CLOSE,
    GLOBAL_ENVS_NUM,
};

typedef struct
{
    struct event_base* base;
    struct evhttp*     http;
    struct event       sig_int;
    struct event       sig_quit;
    struct event       sig_term;
    VALUE              app;
    VALUE              options;
    VALUE              env;
    int                exit;
} Aspirin_Server;

typedef struct
{
    struct evhttp_request* request;
    VALUE              app;
    VALUE              env;
} Aspirin_Response;

extern VALUE rb_mAspirin;
extern VALUE rb_cAspirin_Server;
extern VALUE rb_cAspirin_Response;
extern VALUE rb_cStringIO;
extern VALUE default_env;
extern VALUE global_envs[];

VALUE dupe_default_env();
char* get_status_code_message(int);
VALUE aspirin_server_alloc(VALUE);
VALUE aspirin_server_initialize(VALUE, VALUE, VALUE);
VALUE aspirin_server_start(VALUE);
VALUE aspirin_server_shutdown(VALUE);
void  aspirin_response_start(struct evhttp_request*, VALUE, VALUE);
VALUE aspirin_response_call(VALUE, VALUE);

#endif
