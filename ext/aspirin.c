#include "aspirin.h"

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

static void
init_default_env()
{
    default_env = rb_hash_new();

    rb_hash_aset(default_env, rb_str_new2("rack.multiprocess"), Qfalse);
    rb_hash_aset(default_env, rb_str_new2("rack.multithread"),  Qfalse);
    rb_hash_aset(default_env, rb_str_new2("rack.run_once"),     Qfalse);
    rb_hash_aset(default_env, rb_str_new2("rack.url_scheme"),   rb_obj_freeze(rb_str_new2("http")));
    rb_hash_aset(default_env, rb_str_new2("rack.version"),      rb_obj_freeze(rb_ary_new3(2, INT2FIX(1), INT2FIX(1))));
    rb_hash_aset(default_env, rb_str_new2("GATEWAY_INTERFACE"), rb_obj_freeze(rb_str_new2("CGI/1.2")));
    rb_hash_aset(default_env, rb_str_new2("SCRIPT_NAME"),       rb_obj_freeze(rb_str_new2("")));
    rb_hash_aset(default_env, rb_str_new2("SERVER_PROTOCOL"),   rb_obj_freeze(rb_str_new2("HTTP/1.1")));
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
    char  buf[8 + 1]; // HTTP/x.y
    snprintf(buf, 9, "HTTP/%d.%d", req->major, req->minor);
    rb_hash_aset(env, rb_str_new2("HTTP_VERSION"), rb_obj_freeze(rb_str_new2(buf)));
}

static void
set_path_info(VALUE env, struct evhttp_request *req)
{
    VALUE request_uri, query_string;
    char *buf, *query;
    int   len = strlen(evhttp_request_uri(req));

    buf = xmalloc(len + 1);
    strcpy(buf, evhttp_request_uri(req));

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

    rb_obj_freeze(request_uri);
    rb_obj_freeze(query_string);

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

    TAILQ_FOREACH(header, req->input_headers, next)
    {
        len = strlen(header->key);
        if(len > 0)
        {
            buf = xmalloc(5 + len + 1);

            strncpy(buf, "HTTP_", 5);
            strncpy(buf + 5, header->key, len);
            buf[len + 5] = '\0';
            hyphen_to_under(upcase(buf));

            rb_hash_aset(env, rb_str_new2(buf), rb_obj_freeze(rb_str_new2(header->value)));

            xfree(buf);
        }
    }
}

static VALUE
aspirin_server_create_env(struct evhttp_request *req, Aspirin_Server *srv)
{
    VALUE env, rack_input, strio, remote_host, request_uri, method;

    env = rb_funcall(srv->env, rb_intern("dup"), 0);

    rb_hash_aset(env, rb_str_new2("rack.errors"), rb_gv_get("$stderr"));

    rack_input = rb_str_new((const char*)EVBUFFER_DATA(req->input_buffer),
                            EVBUFFER_LENGTH(req->input_buffer));
    rb_obj_freeze(rack_input);
    strio = rb_funcall(rb_cStringIO, rb_intern("new"), 1, rack_input);
    rb_hash_aset(env, rb_str_new2("rack.input"), strio);

    rb_hash_aset(env,
                 rb_str_new2("REMOTE_ADDR"),
                 rb_obj_freeze(rb_str_new2(req->remote_host)));

    rb_hash_aset(env,
                 rb_str_new2("REQUEST_URI"),
                 rb_obj_freeze(rb_str_new2(evhttp_request_uri(req))));

    method = rb_str_new2("REQUEST_METHOD");
    switch(req->type)
    {
    case EVHTTP_REQ_GET:
        rb_hash_aset(env, method, rb_obj_freeze(rb_str_new2("GET")));
        break;
    case EVHTTP_REQ_POST:
        rb_hash_aset(env, method, rb_obj_freeze(rb_str_new2("POST")));
        break;
    case EVHTTP_REQ_HEAD:
        rb_hash_aset(env, method, rb_obj_freeze(rb_str_new2("HEAD")));
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

static VALUE
body_concat(VALUE chunk, VALUE* buff)
{
    struct evbuffer *buf = DATA_PTR(*buff);
    StringValue(chunk);
    evbuffer_add(buf, RSTRING_PTR(chunk), RSTRING_LEN(chunk));
    return Qnil;
}

static VALUE
body_each(VALUE body)
{
    VALUE data = rb_ary_entry(body, 1);
    rb_iterate(rb_each, rb_ary_entry(body, 0), body_concat, (VALUE)&data);
    return Qnil;
}

static VALUE
body_close(VALUE body)
{
    if(rb_respond_to(body, rb_intern("close")))
    {
        rb_funcall(body, rb_intern("close"), 0);
    }
    return Qnil;
}

static void
aspirin_server_http_request(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;
    Aspirin_Server  *srv;
    VALUE env, result, body, buff;
    int   status_code;
    char *status_code_msg;

    srv = arg;
    env = aspirin_server_create_env(req, srv);
    result = rb_funcall(srv->app, rb_intern("call"), 1, env);

    status_code = NUM2INT(rb_ary_entry(result, 0));
    status_code_msg = get_status_code_message(status_code);
    if(status_code_msg == NULL)
    {
        status_code_msg = "";
    }

    set_response_header(req, rb_ary_entry(result, 1));
    set_additional_header(req);

    buff = Data_Wrap_Struct(rb_cData, 0, 0, 0);
    DATA_PTR(buff) = req->output_buffer;

    body = rb_ary_new3(2, rb_ary_entry(result, 2), buff);

    rb_ensure(body_each, body, body_close, rb_ary_entry(result, 2));
    evhttp_send_reply(req, status_code, status_code_msg, NULL);

    DATA_PTR(buff) = NULL;
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

static VALUE
aspirin_server_alloc(VALUE klass)
{
    return Data_Wrap_Struct(klass, aspirin_server_mark, aspirin_server_free, 0);
}

static void
aspirin_server_stop_bang(int fd, short event, void *arg)
{
    Aspirin_Server *srv = arg;
    event_base_loopbreak(srv->base);
}

static void
aspirin_server_stop(int fd, short event, void *arg)
{
    Aspirin_Server *srv = arg;
    struct timeval delay = {1, 0};
    event_del(&srv->sig_quit);
    event_base_loopexit(srv->base, &delay);
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
aspirin_server_http_initialize(Aspirin_Server *srv)
{
    VALUE host = aspirin_server_host(srv->options);
    int   port = aspirin_server_port(srv->options);

    StringValue(host);
    rb_hash_aset(srv->env, rb_str_new2("SERVER_NAME"), host);

    char port_str[6];
    snprintf(port_str, 5, "%d", port);
    rb_hash_aset(srv->env, rb_str_new2("SERVER_PORT"), rb_str_new2(port_str));

    srv->http = evhttp_new(srv->base);
    evhttp_bind_socket(srv->http, RSTRING_PTR(host), port);
    evhttp_set_gencb(srv->http, aspirin_server_http_request, srv);
}

static char*
upcase(char* str)
{
    if(str){
        char* tmp;
        for(tmp=str; *tmp; tmp++)
        {
            *tmp = toupper(*tmp);
        }
    }
    return str;
}

static char*
hyphen_to_under(char* str)
{
    if(str){
        char* tmp;
        for(tmp=str; *tmp; tmp++)
        {
            if(*tmp == '-')
            {
                *tmp = '_';
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
    VALUE rb_mRack, rb_mRack_Handler, rb_mRack_Handler_Aspirin;

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
    rb_mRack = rb_define_module("Rack");
    rb_mRack_Handler = rb_define_module_under(rb_mRack, "Handler");
    rb_mRack_Handler_Aspirin = rb_define_module_under(rb_mRack_Handler, "Aspirin");
    rb_define_singleton_method(rb_mRack_Handler_Aspirin, "run", rack_handler_aspirin_run, -1);
}
