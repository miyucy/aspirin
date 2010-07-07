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

void  init_default_env();
VALUE dupe_default_env();
void  init_global_envs();
void  init_status_code_tbl();
char* get_status_code_message(int);
VALUE aspirin_server_alloc(VALUE);
void  aspirin_server_mark(Aspirin_Server*);
void  aspirin_server_free(Aspirin_Server*);
VALUE aspirin_server_initialize(VALUE, VALUE, VALUE);
void  aspirin_server_base_initialize(Aspirin_Server*);
void  aspirin_server_signal_initialize(Aspirin_Server*);
void  aspirin_server_stop(int, short, void*);
void  aspirin_server_stop_bang(int, short, void*);
void  aspirin_server_http_initialize(Aspirin_Server*);
VALUE aspirin_server_host(VALUE);
int   aspirin_server_port(VALUE);
void  aspirin_server_http_request(struct evhttp_request*, void*);
VALUE aspirin_server_start(VALUE);
VALUE aspirin_server_shutdown(VALUE);
void  aspirin_response_start(struct evhttp_request*, VALUE, VALUE);
void  aspirin_response_mark(Aspirin_Response*);
VALUE aspirin_response_call_with_catch_async(VALUE, VALUE);
VALUE aspirin_response_call(VALUE, VALUE);
int   aspirin_response_each_header(VALUE, VALUE, VALUE);
void  aspirin_response_set_header(struct evhttp_request*, VALUE);
void  aspirin_response_set_additional_header(struct evhttp_request*);
void  aspirin_response_set_body(struct evhttp_request*, VALUE);
VALUE aspirin_response_each_body(VALUE);
VALUE aspirin_response_write_body(VALUE, VALUE*);
VALUE aspirin_response_close_body(VALUE);
VALUE aspirin_response_create_env(VALUE, VALUE);
void  set_rack_input(VALUE, struct evbuffer*);
void  set_rack_errors(VALUE);
void  set_remote_host(VALUE, char*);
void  set_request_uri(VALUE, const char*);
void  set_request_path(VALUE, const char*);
void  set_parts(VALUE, char*, char, int);
void  set_request_method(VALUE, enum evhttp_cmd_type);
void  set_http_version(VALUE, char, char);
void  set_http_header(VALUE, struct evkeyvalq*);
char* upper_snake(char*);
void  set_async_callback(VALUE, VALUE);


#endif
