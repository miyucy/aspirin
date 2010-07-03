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

enum{
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
} Aspirin_Server;

// Aspirin::Server.new
static VALUE aspirin_server_initialize(VALUE, VALUE, VALUE);
// Aspirin::Server#start
static VALUE aspirin_server_start(VALUE);
// Aspirin::Server#shutdown
static VALUE aspirin_server_shutdown(VALUE);
// Rack::Handler::Aspirin.run
static VALUE rack_handler_aspirin_run(int, VALUE*, VALUE);

static VALUE aspirin_server_host(VALUE);
static int   aspirin_server_port(VALUE);

static VALUE aspirin_server_alloc(VALUE);
static void  aspirin_server_mark(Aspirin_Server*);
static void  aspirin_server_free(Aspirin_Server*);

static void  aspirin_server_http_request(struct evhttp_request*, void*);
static VALUE aspirin_server_create_env(struct evhttp_request*, Aspirin_Server*);

static void  aspirin_server_base_initialize(Aspirin_Server*);
static void  aspirin_server_signal_initialize(Aspirin_Server*);
static void  aspirin_server_stop(int, short, void*);
static void  aspirin_server_stop_bang(int, short, void*);
static void  aspirin_server_http_initialize(Aspirin_Server*);

static void  set_http_version(VALUE, struct evhttp_request*);
static void  set_path_info(VALUE, struct evhttp_request*);
static void  set_response_header(struct evhttp_request*, VALUE);
static void  set_additional_header(struct evhttp_request*);

static void  init_default_env();
static VALUE dupe_default_env();
static void  init_status_code_tbl();
static char* get_status_code_message(int);

static char* upper_snake(char*);
#endif
