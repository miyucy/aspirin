require "mkmf"

dir_config("event")
have_header("event.h")
have_header("evhttp.h")
have_library("event")
create_makefile("aspirin")
