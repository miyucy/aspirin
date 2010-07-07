#include "aspirin.h"

void
aspirin_response_start(struct evhttp_request* request, VALUE app, VALUE env)
{
    Aspirin_Response *response;
    VALUE obj = Data_Make_Struct(rb_cAspirin_Response, Aspirin_Response, 0, -1, response);
    response->request = request;

    VALUE envs = aspirin_response_create_env(obj, env);
    VALUE arg = rb_ary_new3(2, app, envs);
    VALUE ret = rb_catch("async", aspirin_response_call_with_catch_async, arg);
    aspirin_response_call(obj, ret);
}

VALUE
aspirin_response_call_with_catch_async(VALUE tag, VALUE arg)
{
    return rb_funcall(rb_ary_entry(arg, 0), rb_intern("call"), 1, rb_ary_entry(arg, 1));
}

VALUE
aspirin_response_call(VALUE obj, VALUE result)
{
    if(result == Qnil)
    {
        return Qnil;
    }

    Check_Type(result, T_ARRAY);

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
    VALUE headers = rb_ary_entry(result, 1);
    VALUE body = rb_ary_entry(result, 2);
    Data_Get_Struct(obj, Aspirin_Response, response);
    aspirin_response_set_header(response->request, headers);
    aspirin_response_set_body(response->request, body);
    evhttp_send_reply(response->request, status_code, status_msg, NULL);

    return Qnil;
}

void
aspirin_response_set_header(struct evhttp_request* req, VALUE headers)
{
    int i, n;
    VALUE header_keys = rb_funcall(headers, rb_intern("keys"), 0);

    n = RARRAY_LEN(header_keys);
    for(i=0; i<n; i++)
    {
        VALUE key = rb_ary_entry(header_keys, i);
        VALUE val = rb_hash_aref(headers, key);
        StringValue(key);
        StringValue(val);
        if(RSTRING_LEN(key) > 0 && RSTRING_LEN(val) > 0)
        {
            evhttp_remove_header(req->output_headers, StringValueCStr(key));
            evhttp_add_header(req->output_headers, StringValueCStr(key), StringValueCStr(val));
        }
    }

    aspirin_response_set_additional_header(req);
}

void
aspirin_response_set_additional_header(struct evhttp_request* req)
{
    evhttp_remove_header(req->output_headers, "Connection");
    evhttp_add_header(req->output_headers, "Connection", "close");
}

void
aspirin_response_set_body(struct evhttp_request* req, VALUE body)
{
    VALUE buff = Data_Wrap_Struct(rb_cData, 0, 0, req->output_buffer);
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
    Aspirin_Response *response;
    Data_Get_Struct(obj, Aspirin_Response, response);
    struct evhttp_request *req = response->request;
    VALUE env = rb_funcall(default_env, rb_intern("dup"), 0);

    set_rack_input(env, req->input_buffer);
    set_rack_errors(env);
    set_remote_host(env, req->remote_host);
    set_request_path(env, evhttp_request_uri(req));
    set_request_method(env, req->type);
    set_http_version(env, req->major, req->minor);
    set_http_header(env, req->input_headers);
    set_async_callback(env, obj);

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
    VALUE rh = rb_str_new2(remote_host);
    rb_obj_freeze(rh);
    rb_hash_aset(env, global_envs[GE_REMOTE_ADDR], rh);
}

void
set_request_path(VALUE env, const char* rui)
{
    VALUE request_uri;
    char *buf;

    request_uri = rb_str_new2(rui);
    request_uri = rb_obj_freeze(request_uri);
    rb_hash_aset(env, global_envs[GE_REQUEST_URI], request_uri);

    buf = xmalloc(strlen(rui) + 1);
    strcpy(buf, rui);

    set_parts(env, buf, '#', GE_FRAGMENT);
    set_parts(env, buf, '?', GE_QUERY_STRING);

    request_uri = rb_str_new2(buf);
    request_uri = rb_obj_freeze(request_uri);

    rb_hash_aset(env, global_envs[GE_PATH_INFO],    request_uri);
    rb_hash_aset(env, global_envs[GE_REQUEST_PATH], request_uri);

    xfree(buf);
}

void
set_parts(VALUE env, char* uri, char delim, int offset)
{
    char *part = strrchr(uri, delim);
    if(part != NULL)
    {
        *part++ = '\0';
        rb_hash_aset(env, global_envs[offset], rb_obj_freeze(rb_str_new2(part)));
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
    }
}

void
set_http_version(VALUE env, char major, char minor)
{
    char buf[8 + 1]; // HTTP/x.y
    snprintf(buf, sizeof(buf), "HTTP/%d.%d", major, minor);
    rb_hash_aset(env, global_envs[GE_HTTP_VERSION], rb_obj_freeze(rb_str_new2(buf)));
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
            rb_hash_aset(env, rb_str_new2(buf), rb_obj_freeze(rb_str_new2(header->value)));
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
    VALUE method = rb_obj_method(obj, global_envs[GE_CALL]);
    rb_hash_aset(env, global_envs[GE_ASYNC_CALLBACK], method);
}
