#include "aspirin.h"

void
aspirin_response_start(struct evhttp_request* request, VALUE app, VALUE env)
{
    Aspirin_Response *response;
    VALUE obj = Data_Make_Struct(rb_cAspirin_Response, Aspirin_Response, aspirin_response_mark, -1, response);

    response->request = request;
    response->app = app;
    response->env = aspirin_response_create_env(obj, env);

    VALUE arg = rb_ary_new3(2, response->app, response->env);
    VALUE ret = rb_catch("async", aspirin_response_call_with_catch_async, arg);
    aspirin_response_call(obj, ret);
}

void
aspirin_response_mark(Aspirin_Response* res)
{
    if(!res)
    {
        return;
    }
    rb_gc_mark(res->app);
    rb_gc_mark(res->env);
}

VALUE
aspirin_response_call_with_catch_async(VALUE tag, VALUE arg)
{
    VALUE app = rb_ary_entry(arg, 0);
    VALUE env = rb_ary_entry(arg, 1);
    return rb_funcall(app, rb_intern("call"), 1, env);
}

VALUE
aspirin_response_call(VALUE obj, VALUE result)
{
    // throw :async
    if(result == Qnil)
    {
        return Qnil;
    }

    Check_Type(result, T_ARRAY);

    // AsyncResponse
    int status_code = NUM2INT(rb_ary_entry(result, 0));
    if(status_code == -1)
    {
        return Qnil;
    }

    char* status_msg = get_status_code_message(status_code);
    if(status_msg == NULL)
    {
        status_msg = "";
    }

    Aspirin_Response *response;
    Data_Get_Struct(obj, Aspirin_Response, response);

    VALUE headers = rb_ary_entry(result, 1);
    aspirin_response_set_header(response->request, headers);

    VALUE body = rb_ary_entry(result, 2);
    aspirin_response_set_body(response->request, body);

    evhttp_send_reply(response->request, status_code, status_msg, NULL);

    return Qnil;
}

int
aspirin_response_each_header(VALUE _key, VALUE _val, VALUE headers)
{
    VALUE key = rb_str_to_str(_key);
    VALUE val = rb_str_to_str(_val);
    if(RSTRING_LEN(key) > 0 && RSTRING_LEN(val) > 0)
    {
        evhttp_remove_header(DATA_PTR(headers), RSTRING_PTR(key));
        evhttp_add_header(DATA_PTR(headers), RSTRING_PTR(key), RSTRING_PTR(val));
    }
    return ST_CONTINUE;
}

void
aspirin_response_set_header(struct evhttp_request* request, VALUE headers)
{
    VALUE output_headers = Data_Wrap_Struct(rb_cData, 0, 0, request->output_headers);
    rb_hash_foreach(headers, aspirin_response_each_header, output_headers);
    DATA_PTR(output_headers) = NULL;
    aspirin_response_set_additional_header(request);
}

void
aspirin_response_set_additional_header(struct evhttp_request* request)
{
    evhttp_remove_header(request->output_headers, "Connection");
    evhttp_add_header(request->output_headers, "Connection", "close");
}

void
aspirin_response_set_body(struct evhttp_request* request, VALUE body)
{
    VALUE buff = Data_Wrap_Struct(rb_cData, 0, 0, request->output_buffer);
    VALUE args = rb_ary_new3(2, body, buff);
    rb_ensure(aspirin_response_each_body, args, aspirin_response_close_body, body);
    DATA_PTR(buff) = NULL;
}

VALUE
aspirin_response_each_body(VALUE args)
{
    VALUE body = rb_ary_entry(args, 0);
    VALUE buff = rb_ary_entry(args, 1);
    rb_iterate(rb_each, body, aspirin_response_write_body, (VALUE)&buff);
    return Qnil;
}

VALUE
aspirin_response_write_body(VALUE chunk, VALUE* buff)
{
    StringValue(chunk);
    evbuffer_add(DATA_PTR(*buff), RSTRING_PTR(chunk), RSTRING_LEN(chunk));
    return Qnil;
}

VALUE
aspirin_response_close_body(VALUE body)
{
    if(rb_respond_to(body, rb_intern("close")))
    {
        rb_funcall(body, rb_intern("close"), 0);
    }
    return Qnil;
}

VALUE
aspirin_response_create_env(VALUE obj, VALUE default_env)
{
    VALUE env = rb_obj_dup(default_env);

    Aspirin_Response *response;
    Data_Get_Struct(obj, Aspirin_Response, response);

    struct evhttp_request *request = response->request;

    set_rack_input(env, request->input_buffer);
    set_rack_errors(env);
    set_request_method(env, request->type);
    set_remote_host(env, request->remote_host);
    set_http_version(env, request->major, request->minor);
    set_async_callback(env, obj);
    set_request_uri(env, evhttp_request_uri(request));
    set_request_path(env, evhttp_request_uri(request));
    set_http_header(env, request->input_headers);

    return env;
}

void
set_rack_input(VALUE env, struct evbuffer* evbuffer)
{
    VALUE str = rb_str_new((const char*)EVBUFFER_DATA(evbuffer),
                           EVBUFFER_LENGTH(evbuffer));
    rb_obj_freeze(str);
    VALUE ret = rb_funcall(rb_cStringIO, rb_intern("new"), 1, str);
    rb_hash_aset(env, global_envs[GE_RACK_INPUT], ret);
}

void
set_rack_errors(VALUE env)
{
    rb_hash_aset(env, global_envs[GE_RACK_ERRORS], rb_gv_get("$stderr"));
}

void
set_remote_host(VALUE env, char* remote_host)
{
    rb_hash_aset(env, global_envs[GE_REMOTE_ADDR], FRZSTR(remote_host));
}

void
set_request_uri(VALUE env, const char* rui)
{
    rb_hash_aset(env, global_envs[GE_REQUEST_URI], FRZSTR(rui));
}

void
set_request_path(VALUE env, const char* rui)
{
    VALUE request_path;
    char *buf = xmalloc(strlen(rui) + 1);
    strcpy(buf, rui);

    set_parts(env, buf, '#', GE_FRAGMENT);
    set_parts(env, buf, '?', GE_QUERY_STRING);

    request_path = FRZSTR(buf);

    rb_hash_aset(env, global_envs[GE_PATH_INFO],    request_path);
    rb_hash_aset(env, global_envs[GE_REQUEST_PATH], request_path);

    xfree(buf);
}

void
set_parts(VALUE env, char* uri, char delim, int offset)
{
    char *part = strrchr(uri, delim);
    if(part != NULL)
    {
        *part++ = '\0';
        rb_hash_aset(env, global_envs[offset], FRZSTR(part));
    }
    else
    {
        rb_hash_aset(env, global_envs[offset], global_envs[GE_EMPTY]);
    }
}

void
set_request_method(VALUE env, enum evhttp_cmd_type type)
{
    switch(type)
    {
    case EVHTTP_REQ_GET:
        rb_hash_aset(env, global_envs[GE_REQUEST_METHOD], global_envs[GE_GET]);
        break;
    case EVHTTP_REQ_POST:
        rb_hash_aset(env, global_envs[GE_REQUEST_METHOD], global_envs[GE_POST]);
        break;
    case EVHTTP_REQ_HEAD:
        rb_hash_aset(env, global_envs[GE_REQUEST_METHOD], global_envs[GE_HEAD]);
        break;
    default:
        rb_raise(rb_eFatal, "unknown evhttp_cmd_type");
        break;
    }
}

void
set_http_version(VALUE env, char major, char minor)
{
    char buf[9];
    snprintf(buf, sizeof(buf), "HTTP/%d.%d", major, minor);
    rb_hash_aset(env, global_envs[GE_HTTP_VERSION], FRZSTR(buf));
}

void
set_http_header(VALUE env, struct evkeyvalq* headers)
{
    char  *buf;
    size_t len;
    struct evkeyval *header;

    TAILQ_FOREACH(header, headers, next)
    {
        len = strlen(header->key);
        if(len > 0)
        {
            buf = xmalloc(5 + len + 1);
            upper_snake(strcpy(strcpy(buf, "HTTP_") + 5, header->key));
            rb_hash_aset(env, rb_str_new2(buf), FRZSTR(header->value));
            xfree(buf);
        }
    }
}

char*
upper_snake(char* str)
{
    if(str){
        char* tmp;
        for(tmp=str; *tmp; tmp++)
        {
            if(*tmp == '-')
            {
                *tmp = '_';
            }
            else
            {
                *tmp = toupper(*tmp);
            }
        }
    }
    return str;
}

void
set_async_callback(VALUE env, VALUE obj)
{
    rb_hash_aset(env,
                 global_envs[GE_ASYNC_CALLBACK],
                 rb_obj_method(obj, ID2SYM(rb_intern("call"))));
}
