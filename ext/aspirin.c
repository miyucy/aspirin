#include "aspirin.h"

VALUE rb_mAspirin;
VALUE rb_cAspirin_Server;
VALUE rb_cAspirin_Response;
VALUE rb_cStringIO;
static struct st_table *status_code_tbl;
static const int const status_code_int[] = {
    100,101,102,200,201,202,203,204,205,206,207,208,226,300,301,302,303,304,305,306,307,400,401,402,403,404,405,406,407,408,409,410,411,412,413,414,415,416,417,420,421,422,423,424,425,426,500,501,502,503,504,505,506,507,508,510
};
static const char* const status_code_str[] = {
    "Continue","Switching Protocols","Processing","OK","Created","Accepted","Non-Authoritative Information","No Content","Reset Content","Partial Content","Multi-Status","Already Reported","IM Used","Multiple Choices","Moved Permanently","Found","See Other","Not Modified","Use Proxy","","Temporary Redirect","Bad Request","Unauthorized","Payment Required","Forbidden","Not Found","Method Not Allowed","Not Acceptable","Proxy Authentication Required","Request Timeout","Conflict","Gone","Length Required","Precondition Failed","Request Entity Too Large","Request-URI Too Long","Unsupported Media Type","Requested Range Not Satisfiable","Expectation Failed","","","Unprocessable Entity","Locked","Failed Dependency","","Upgrade Required","Internal Server Error","Not Implemented","Bad Gateway","Service Unavailable","Gateway Timeout","HTTP Version Not Supported","Variant Also Negotiates","Insufficient Storage","Loop Detected","Not Extended"
};
VALUE default_env;
VALUE global_envs[GLOBAL_ENVS_NUM];

void
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

VALUE
dupe_default_env()
{
    return rb_funcall(default_env, rb_intern("dup"), 0);
}

void
init_global_envs()
{
    int i;
    global_envs[GE_GET           ] = rb_str_new2("GET");
    global_envs[GE_HEAD          ] = rb_str_new2("HEAD");
    global_envs[GE_HTTP_VERSION  ] = rb_str_new2("HTTP_VERSION");
    global_envs[GE_PATH_INFO     ] = rb_str_new2("PATH_INFO");
    global_envs[GE_POST          ] = rb_str_new2("POST");
    global_envs[GE_QUERY_STRING  ] = rb_str_new2("QUERY_STRING");
    global_envs[GE_RACK_ERRORS   ] = rb_str_new2("rack.errors");
    global_envs[GE_RACK_INPUT    ] = rb_str_new2("rack.input");
    global_envs[GE_REMOTE_ADDR   ] = rb_str_new2("REMOTE_ADDR");
    global_envs[GE_REQUEST_METHOD] = rb_str_new2("REQUEST_METHOD");
    global_envs[GE_REQUEST_PATH  ] = rb_str_new2("REQUEST_PATH");
    global_envs[GE_REQUEST_URI   ] = rb_str_new2("REQUEST_URI");
    global_envs[GE_SERVER_NAME   ] = rb_str_new2("SERVER_NAME");
    global_envs[GE_SERVER_PORT   ] = rb_str_new2("SERVER_PORT");
    global_envs[GE_FRAGMENT      ] = rb_str_new2("FRAGMENT");
    global_envs[GE_EMPTY         ] = rb_str_new2("");
    for(i=0; i<GLOBAL_ENVS_NUM; i++)
    {
        rb_gc_mark(rb_obj_freeze(global_envs[i]));
    }
}

void
init_status_code_tbl()
{
    int i, n = sizeof(status_code_int)/sizeof(int);
    status_code_tbl = st_init_numtable();
    for(i=0; i<n; i++)
    {
        st_add_direct(status_code_tbl, (st_data_t)status_code_int[i], (st_data_t)status_code_str[i]);
    }
}

char*
get_status_code_message(int status_code)
{
    char* result = NULL;
    st_lookup(status_code_tbl, (st_data_t)status_code, (st_data_t*)&result);
    return result;
}

VALUE
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

    init_global_envs();
    init_default_env();
    init_status_code_tbl();

    rb_require("stringio");
    rb_cStringIO = rb_const_get(rb_cObject, rb_intern("StringIO"));

    rb_mAspirin          = rb_define_module("Aspirin");
    rb_cAspirin_Server   = rb_define_class_under(rb_mAspirin, "Server", rb_cObject);
    rb_cAspirin_Response = rb_define_class_under(rb_mAspirin, "Response", rb_cObject);

    // Aspirin::Server
    rb_define_alloc_func(rb_cAspirin_Server, aspirin_server_alloc);
    rb_define_method(rb_cAspirin_Server, "initialize", aspirin_server_initialize, 2);
    rb_define_method(rb_cAspirin_Server, "start", aspirin_server_start, 0);
    rb_define_method(rb_cAspirin_Server, "shutdown", aspirin_server_shutdown, 0);

    // Aspirin::Response
    rb_undef_alloc_func(rb_cAspirin_Response);
    rb_define_method(rb_cAspirin_Response, "call", aspirin_response_call, 1);

    // Rack::Handler::Aspirin
    rb_mRack = rb_define_module("Rack");
    rb_mRack_Handler = rb_define_module_under(rb_mRack, "Handler");
    rb_mRack_Handler_Aspirin = rb_define_module_under(rb_mRack_Handler, "Aspirin");
    rb_define_singleton_method(rb_mRack_Handler_Aspirin, "run", rack_handler_aspirin_run, -1);
    // rb_mRack_Handler.register
    if(rb_respond_to(rb_mRack_Handler, rb_intern("register")))
    {
        rb_funcall(rb_mRack_Handler, rb_intern("register"),
                   2, rb_str_new2("aspirin"), rb_str_new2("Rack::Handler::Aspirin"));
    }
}
