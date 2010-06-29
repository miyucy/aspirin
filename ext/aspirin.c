#include <ruby.h>
#ifdef HAVE_RUBY_ST_H
#include <ruby/st.h>
#else
#include <st.h>
#endif
#include <signal.h>
#include <sys/queue.h>
#include <stdio.h>
#include <ctype.h>

#include <event.h>
#include <evhttp.h>
#include <evutil.h>

#define INSPECT(obj) {VALUE __obj__ = rb_inspect((obj)); fprintf(stderr, "%d:%s\n", __LINE__, RSTRING_PTR(__obj__));}

static VALUE rb_mAspirin;
static VALUE rb_cAspirin_Server;
static VALUE rb_cStringIO;
static struct st_table *status_code_tbl;
static const int const status_code_int[] = {
    100,101,102,200,201,202,203,204,205,206,207,208,226,300,301,302,303,304,305,306,307,400,401,402,403,404,405,406,407,408,409,410,411,412,413,414,415,416,417,420,421,422,423,424,425,426,500,501,502,503,504,505,506,507,508,510
};
static const char* const status_code_str[] = {
    "Continue","Switching Protocols","Processing","OK","Created","Accepted","Non-Authoritative Information","No Content","Reset Content","Partial Content","Multi-Status","Already Reported","IM Used","Multiple Choices","Moved Permanently","Found","See Other","Not Modified","Use Proxy","","Temporary Redirect","Bad Request","Unauthorized","Payment Required","Forbidden","Not Found","Method Not Allowed","Not Acceptable","Proxy Authentication Required","Request Timeout","Conflict","Gone","Length Required","Precondition Failed","Request Entity Too Large","Request-URI Too Long","Unsupported Media Type","Requested Range Not Satisfiable","Expectation Failed","","","Unprocessable Entity","Locked","Failed Dependency","","Upgrade Required","Internal Server Error","Not Implemented","Bad Gateway","Service Unavailable","Gateway Timeout","HTTP Version Not Supported","Variant Also Negotiates","Insufficient Storage","Loop Detected","Not Extended"
};
static VALUE default_env;

typedef struct
{
    struct event_base* base;
    struct evhttp*     http;
    struct event       signal;
    VALUE              app;
    VALUE              options;
    VALUE              env;
} Aspirin_Server;

// Aspirin::Server.new
static VALUE aspirin_server_initialize(VALUE, VALUE, VALUE);
// Aspirin::Server.start
static VALUE aspirin_server_start(VALUE);
// Rack::Handler::Aspirin.run
static VALUE rack_handler_aspirin_run(int, VALUE*, VALUE);

static VALUE aspirin_server_address(VALUE);
static int   aspirin_server_port(VALUE);

static VALUE aspirin_server_alloc(VALUE);
static void  aspirin_server_mark(Aspirin_Server*);
static void  aspirin_server_free(Aspirin_Server*);

static void  aspirin_server_http_request(struct evhttp_request*, void*);
static VALUE aspirin_server_create_env(struct evhttp_request*, Aspirin_Server*);

static void  aspirin_server_base_initialize(Aspirin_Server*);
static void  aspirin_server_signal_initialize(Aspirin_Server*);
static void  aspirin_server_sigint(int, short, void*);
static void  aspirin_server_http_initialize(Aspirin_Server*);

static void  set_http_version(VALUE, struct evhttp_request*);
static void  set_path_info(VALUE, struct evhttp_request*);

static void  init_default_env();
static VALUE dupe_default_env();
static void  init_status_code_tbl();
static char* get_status_code_message(int);

static char* upcase(char*);
static char* hyphen_to_under(char*);

static void
init_default_env()
{
    VALUE rack_url_scheme, rack_version, gateway_interface, server_protocol, script_name;

    rack_url_scheme   = rb_str_new2("http");
    rack_version      = rb_ary_new3(2, INT2FIX(1), INT2FIX(1));
    gateway_interface = rb_str_new2("CGI/1.2");
    server_protocol   = rb_str_new2("HTTP/1.1");
    script_name       = rb_str_new2("");

    OBJ_FREEZE(rack_url_scheme);
    OBJ_FREEZE(rack_version);
    OBJ_FREEZE(gateway_interface);
    OBJ_FREEZE(server_protocol);
    OBJ_FREEZE(script_name);

    default_env = rb_hash_new();

    rb_hash_aset(default_env, rb_str_new2("rack.multiprocess"), Qfalse);
    rb_hash_aset(default_env, rb_str_new2("rack.multithread"),  Qfalse);
    rb_hash_aset(default_env, rb_str_new2("rack.run_once"),     Qfalse);
    rb_hash_aset(default_env, rb_str_new2("rack.url_scheme"),   rack_url_scheme);
    rb_hash_aset(default_env, rb_str_new2("rack.version"),      rack_version);
    rb_hash_aset(default_env, rb_str_new2("GATEWAY_INTERFACE"), gateway_interface);
    rb_hash_aset(default_env, rb_str_new2("SCRIPT_NAME"),       script_name);
    rb_hash_aset(default_env, rb_str_new2("SERVER_PROTOCOL"),   server_protocol);
}

static VALUE
dupe_default_env()
{
    return rb_funcall(default_env, rb_intern("dup"), 0);
}

static void
init_status_code_tbl()
{
    int i, n = sizeof(status_code_int)/sizeof(int);
    status_code_tbl = st_init_numtable();
    for(i=0; i<n; i++)
    {
        st_add_direct(status_code_tbl, (st_data_t)status_code_int[i], (st_data_t)status_code_str[i]);
    }
}

static char*
get_status_code_message(int status_code)
{
    char* result = NULL;
    st_lookup(status_code_tbl, (st_data_t)status_code, (st_data_t*)&result);
    return result;
}

static void
set_http_version(VALUE env, struct evhttp_request *req)
{
    char buff[9];
    snprintf(buff, 8, "HTTP/%d.%d", req->major, req->minor);
    VALUE val = rb_str_new(buff, 8);
    OBJ_FREEZE(val);
    rb_hash_aset(env, rb_str_new2("HTTP_VERSION"), val);
}

static void
set_path_info(VALUE env, struct evhttp_request *req)
{
    VALUE request_uri, query_string;
    char *buf, *query;
    int   len = strlen(evhttp_request_uri(req));

    buf = xmalloc(len * sizeof(char) + 1);
    memcpy(buf, evhttp_request_uri(req), len + 1);

    query = strrchr(buf, '?');
    if(query != NULL)
    {
        *query++ = '\0';
    }
    else
    {
        query = "";
    }

    request_uri  = rb_str_new2(buf);
    query_string = rb_str_new2(query);

    OBJ_FREEZE(request_uri);
    OBJ_FREEZE(query_string);

    rb_hash_aset(env, rb_str_new2("PATH_INFO"), request_uri);
    rb_hash_aset(env, rb_str_new2("REQUEST_PATH"), request_uri);
    rb_hash_aset(env, rb_str_new2("QUERY_STRING"), query_string);

    xfree(buf);
}

static void
set_http_header(VALUE env, struct evhttp_request *req)
{
    char  *buf;
    size_t len;
    struct evkeyval *header;
    VALUE key, val;

    TAILQ_FOREACH(header, req->input_headers, next)
    {
        len = strlen(header->key);
        if(len > 0)
        {
            buf = xmalloc(len * sizeof(char) + 5 + 1);

            strncpy(buf, "HTTP_", 5);
            strncpy(buf + 5, header->key, len);
            hyphen_to_under(upcase(buf));

            key = rb_str_new(buf, len + 5);
            val = rb_str_new2(header->value);
            OBJ_FREEZE(val);
            rb_hash_aset(env, key, val);

            xfree(buf);
        }
    }
}

static VALUE
aspirin_server_create_env(struct evhttp_request *req, Aspirin_Server *srv)
{
    VALUE env, rack_input, strio, remote_host, request_uri, rbstrerr, method;

    env = rb_funcall(srv->env, rb_intern("dup"), 0);
    rbstrerr = rb_gv_get("$stderr");
    rb_hash_aset(env, rb_str_new2("rack.errors"), rbstrerr);

    rack_input = rb_str_new((const char*)EVBUFFER_DATA(req->input_buffer),
                            EVBUFFER_LENGTH(req->input_buffer));
    OBJ_FREEZE(rack_input);
    strio = rb_funcall(rb_cStringIO, rb_intern("new"), 1, rack_input);
    rb_hash_aset(env, rb_str_new2("rack.input"), strio);

    //fprintf(stderr, "%d:%s\n", __LINE__, req->remote_host);
    remote_host = rb_str_new2(req->remote_host);
    OBJ_FREEZE(remote_host);
    rb_hash_aset(env, rb_str_new2("REMOTE_ADDR"), remote_host);

    request_uri = rb_str_new2(evhttp_request_uri(req));
    OBJ_FREEZE(request_uri);
    rb_hash_aset(env, rb_str_new2("REQUEST_URI"), request_uri);

    method = rb_str_new2("REQUEST_METHOD");
    switch(req->type)
    {
    case EVHTTP_REQ_GET:
        rb_hash_aset(env, method, rb_str_new2("GET"));
        break;
    case EVHTTP_REQ_POST:
        rb_hash_aset(env, method, rb_str_new2("POST"));
        break;
    case EVHTTP_REQ_HEAD:
        rb_hash_aset(env, method, rb_str_new2("HEAD"));
        break;
    }

    set_http_version(env, req);
    set_path_info(env, req);
    set_http_header(env, req);

    return env;
}

static void
set_response_header(struct evhttp_request *req, VALUE headers)
{
    VALUE header_keys, key, val;
    int i, n;

    header_keys = rb_funcall(headers, rb_intern("keys"), 0);

    n = RARRAY_LEN(header_keys);
    for(i=0; i<n; i++)
    {
        key = rb_ary_entry(header_keys, i);
        val = rb_hash_aref(headers, key);
        StringValue(key);
        StringValue(val);
        if(RSTRING_LEN(key) > 0 && RSTRING_LEN(val) > 0)
        {
            evhttp_remove_header(req->output_headers, StringValueCStr(key));
            evhttp_add_header(req->output_headers, StringValueCStr(key), StringValueCStr(val));
        }
    }
}

static void
set_additional_header(struct evhttp_request *req)
{
    evhttp_remove_header(req->output_headers, "Connection");
    evhttp_add_header(req->output_headers, "Connection", "close");
}

static void
aspirin_server_http_request(struct evhttp_request *req, void *arg)
{
    static VALUE args[][1] = {{INT2FIX(0)},{INT2FIX(1)},{INT2FIX(2)}};
    struct evbuffer *buf;
    Aspirin_Server  *srv;
    VALUE env, result, status_code, bodies_val, bodies, headers, guard;
    char *status_code_msg;

    srv = arg;
    env = aspirin_server_create_env(req, srv);
    result = rb_funcall(srv->app, rb_intern("call"), 1, env);

    status_code = rb_ary_aref(1, args[0], result);
    status_code_msg = get_status_code_message(NUM2INT(status_code));
    if(status_code_msg == NULL)
    {
        status_code_msg = "";
    }

    headers = rb_ary_aref(1, args[1], result);
    set_response_header(req, headers);
    set_additional_header(req);

    guard = rb_ary_new();
    rb_gc_register_address(&guard);

    bodies_val = rb_ary_aref(1, args[2], result);
    bodies = rb_funcall(bodies_val, rb_intern("to_s"), 0);

    buf = evbuffer_new();
    evbuffer_add(buf, RSTRING_PTR(bodies), RSTRING_LEN(bodies));
    rb_ary_push(guard, bodies);
    evhttp_send_reply(req, NUM2INT(status_code), status_code_msg, buf);
    evbuffer_free(buf);

    rb_gc_unregister_address(&guard);

    // rb_gc_force_recycle(env);
    // rb_gc_start();
}

static Aspirin_Server*
check_aspirin_server(VALUE obj)
{
    Check_Type(obj, T_DATA);
    return DATA_PTR(obj);
}

static Aspirin_Server*
alloc_aspirin_server()
{
    Aspirin_Server *srv = ALLOC(Aspirin_Server);
    memset(srv, 0, sizeof(Aspirin_Server));
    return srv;
}


static VALUE
aspirin_server_initialize(VALUE obj, VALUE app, VALUE options)
{
    Aspirin_Server *srv = check_aspirin_server(obj);

    if(!srv)
    {
        DATA_PTR(obj) = srv = alloc_aspirin_server();
    }

    srv->app = app;
    srv->options = options;
    srv->env = dupe_default_env();

    aspirin_server_base_initialize(srv);
    aspirin_server_signal_initialize(srv);
    aspirin_server_http_initialize(srv);

    return obj;
}

static VALUE
aspirin_server_start(VALUE obj)
{
    Aspirin_Server *srv = DATA_PTR(obj);
    event_base_loop(srv->base, 0);
    return Qnil;
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

    event_del(&srv->signal);

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

static VALUE
aspirin_server_alloc(VALUE klass)
{
    return Data_Wrap_Struct(klass, aspirin_server_mark, aspirin_server_free, 0);
}

static void
aspirin_server_sigint(int fd, short event, void *arg)
{
    Aspirin_Server *srv = arg;
    struct timeval delay = {1, 0};
    event_del(&srv->signal);
    event_base_loopexit(srv->base, &delay);
}

static int
aspirin_server_port(VALUE options)
{
    VALUE port = Qnil;
    if(TYPE(options) == T_HASH)
    {
        port = rb_hash_aref(options, ID2SYM(rb_intern("port")));
        if(!NIL_P(port)){
            return NUM2INT(port);
        }
        port = rb_hash_aref(options, rb_str_new2("port"));
        if(!NIL_P(port)){
            return NUM2INT(port);
        }
        port = rb_hash_aref(options, ID2SYM(rb_intern("Port")));
        if(!NIL_P(port)){
            return NUM2INT(port);
        }
        port = rb_hash_aref(options, rb_str_new2("Port"));
        if(!NIL_P(port)){
            return NUM2INT(port);
        }
        port = rb_hash_aref(options, ID2SYM(rb_intern("PORT")));
        if(!NIL_P(port)){
            return NUM2INT(port);
        }
        port = rb_hash_aref(options, rb_str_new2("PORT"));
        if(!NIL_P(port)){
            return NUM2INT(port);
        }
    }
    return 9292;
}

static VALUE
aspirin_server_address(VALUE options)
{
    VALUE address = Qnil;
    if(TYPE(options) == T_HASH)
    {
        address = rb_hash_aref(options, ID2SYM(rb_intern("address")));
        if(!NIL_P(address)){
            return address;
        }
        address = rb_hash_aref(options, rb_str_new2("address"));
        if(!NIL_P(address)){
            return address;
        }
        address = rb_hash_aref(options, ID2SYM(rb_intern("Address")));
        if(!NIL_P(address)){
            return address;
        }
        address = rb_hash_aref(options, rb_str_new2("Address"));
        if(!NIL_P(address)){
            return address;
        }
        address = rb_hash_aref(options, ID2SYM(rb_intern("ADDRESS")));
        if(!NIL_P(address)){
            return address;
        }
        address = rb_hash_aref(options, rb_str_new2("ADDRESS"));
        if(!NIL_P(address)){
            return address;
        }
    }
    return rb_str_new2("0.0.0.0");
}

static void
aspirin_server_base_initialize(Aspirin_Server *srv)
{
    srv->base = event_base_new();
}

static void
aspirin_server_signal_initialize(Aspirin_Server *srv)
{
    event_set(&srv->signal, SIGINT, EV_SIGNAL|EV_PERSIST, aspirin_server_sigint, srv);
    event_base_set(srv->base, &srv->signal);
    event_add(&srv->signal, NULL);
}

static void
aspirin_server_http_initialize(Aspirin_Server *srv)
{
    VALUE addr = aspirin_server_address(srv->options);
    int   port = aspirin_server_port(srv->options);

    StringValue(addr);
    rb_hash_aset(srv->env, rb_str_new2("SERVER_NAME"), addr);

    char port_str[6];
    snprintf(port_str, 5, "%d", port);
    rb_hash_aset(srv->env, rb_str_new2("SERVER_PORT"), rb_str_new2(port_str));

    srv->http = evhttp_new(srv->base);
    evhttp_bind_socket(srv->http, RSTRING_PTR(addr), port);
    evhttp_set_gencb(srv->http, aspirin_server_http_request, srv);
}

static char*
upcase(char* str)
{
    if(str){
        int i, n=strlen(str);
        for(i=0; i<n; i++)
        {
            str[i] = toupper(str[i]);
        }
    }
    return str;
}

static char*
hyphen_to_under(char* str)
{
    if(str){
        int i, n=strlen(str);
        for(i=0; i<n; i++)
        {
            if(str[i] == '-')
            {
                str[i] = '_';
            }
        }
    }
    return str;
}

static VALUE
rack_handler_aspirin_run(int argc, VALUE *argv, VALUE obj)
{
    VALUE app, options;
    if(rb_scan_args(argc, argv, "11", &app, &options) == 1)
    {
        options = rb_hash_new();
    }
    aspirin_server_start(rb_funcall(rb_cAspirin_Server, rb_intern("new"), 2, app, options));
    return Qnil;
}

void Init_aspirin(void)
{
    init_default_env();
    init_status_code_tbl();

    rb_require("stringio");
    rb_cStringIO = rb_const_get(rb_cObject, rb_intern("StringIO"));

    rb_mAspirin         = rb_define_module("Aspirin");
    rb_cAspirin_Server  = rb_define_class_under(rb_mAspirin, "Server", rb_cObject);

    // Aspirin::Server
    rb_define_alloc_func(rb_cAspirin_Server, aspirin_server_alloc);
    rb_define_method(rb_cAspirin_Server, "initialize", aspirin_server_initialize, 2);
    rb_define_method(rb_cAspirin_Server, "start", aspirin_server_start, 0);

    // Rack::Handler::Aspirin
    VALUE rb_mRack = rb_define_module("Rack");
    VALUE rb_mRack_Handler = rb_define_module_under(rb_mRack, "Handler");
    VALUE rb_mRack_Handler_Aspirin = rb_define_module_under(rb_mRack_Handler, "Aspirin");
    rb_define_singleton_method(rb_mRack_Handler_Aspirin, "run", rack_handler_aspirin_run, -1);
}
