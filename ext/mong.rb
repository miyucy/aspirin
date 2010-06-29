# -*- coding: utf-8 -*-
require 'rubygems'
require 'rack'
require 'pp'

class HelloApp
  def call(env)
    # env.keys.sort.each do |k|
    #   v = env[k]
    #   puts "#{k.inspect} => #{v.inspect}"
    # end
    [200, {"Content-Type" => "text/plain"}, ["Hello, Rack"]]
  end
end
Rack::Handler::Mongrel.run HelloApp.new
__END__
∴ curl "localhost:8080/path/to/file?arg1=1&arg2=2"
"GATEWAY_INTERFACE" => "CGI/1.2"
"HTTP_ACCEPT" => "*/*"
"HTTP_HOST" => "localhost:8080"
"HTTP_USER_AGENT" => "curl/7.19.7 (i486-pc-linux-gnu) libcurl/7.19.7 OpenSSL/0.9.8k zlib/1.2.3.3 libidn/1.15"
"HTTP_VERSION" => "HTTP/1.1"
"PATH_INFO" => "/path/to/file"
"QUERY_STRING" => "arg1=1&arg2=2"
"REMOTE_ADDR" => "127.0.0.1"
"REQUEST_METHOD" => "GET"
"REQUEST_PATH" => "/path/to/file"
"REQUEST_URI" => "/path/to/file?arg1=1&arg2=2"
"SCRIPT_NAME" => ""
"SERVER_NAME" => "localhost"
"SERVER_PORT" => "8080"
"SERVER_PROTOCOL" => "HTTP/1.1"
"SERVER_SOFTWARE" => "Mongrel 1.1.5"
"rack.errors" => #<IO:0xb77b8560>
"rack.input" => #<StringIO:0xb708bb34>
"rack.multiprocess" => false
"rack.multithread" => true
"rack.run_once" => false
"rack.url_scheme" => "http"
"rack.version" => [1, 1]

∴ ab -n 1000 -c 10 http://127.0.0.1:8080/
This is ApacheBench, Version 2.3 <$Revision: 655654 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 127.0.0.1 (be patient)
Completed 100 requests
Completed 200 requests
Completed 300 requests
Completed 400 requests
Completed 500 requests
Completed 600 requests
Completed 700 requests
Completed 800 requests
Completed 900 requests
Completed 1000 requests
Finished 1000 requests


Server Software:
Server Hostname:        127.0.0.1
Server Port:            8080

Document Path:          /
Document Length:        11 bytes

Concurrency Level:      10
Time taken for tests:   1.236 seconds
Complete requests:      1000
Failed requests:        0
Write errors:           0
Total transferred:      132000 bytes
HTML transferred:       11000 bytes
Requests per second:    808.99 [#/sec] (mean)
Time per request:       12.361 [ms] (mean)
Time per request:       1.236 [ms] (mean, across all concurrent requests)
Transfer rate:          104.28 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.0      0       0
Processing:     5   12   8.5      9      58
Waiting:        5   12   8.5      9      58
Total:          5   12   8.5      9      58

Percentage of the requests served within a certain time (ms)
  50%      9
  66%     10
  75%     10
  80%     11
  90%     24
  95%     30
  98%     43
  99%     56
 100%     58 (longest request)
