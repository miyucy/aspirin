# -*- coding: utf-8 -*-
require 'rubygems'
require 'thin'
require 'rack'

class HelloApp
  def call(env)
    # env.keys.sort.each do |k|
    #   v = env[k]
    #   puts "#{k.inspect} => #{v.inspect}"
    # end
    [200, {"Content-Type" => "text/plain"}, ["Hello, Rack"]]
  end
end
Rack::Handler::Thin.run HelloApp.new
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
"SERVER_SOFTWARE" => "thin 1.2.7 codename No Hup"
"async.callback" => #<Method: Thin::Connection#post_process>
"async.close" => #<EventMachine::DefaultDeferrable:0x966bb5c>
"rack.errors" => #<IO:0x92f4550>
"rack.input" => #<StringIO:0x966bdb4>
"rack.multiprocess" => false
"rack.multithread" => false
"rack.run_once" => false
"rack.url_scheme" => "http"
"rack.version" => [1, 0]

∴ ab -n 10000 -c 100 http://127.0.0.1:8080/ 2>/dev/null
This is ApacheBench, Version 2.3 <$Revision: 655654 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 127.0.0.1 (be patient)


Server Software:        thin
Server Hostname:        127.0.0.1
Server Port:            8080

Document Path:          /
Document Length:        11 bytes

Concurrency Level:      100
Time taken for tests:   6.314 seconds
Complete requests:      10000
Failed requests:        0
Write errors:           0
Total transferred:      1310000 bytes
HTML transferred:       110000 bytes
Requests per second:    1583.76 [#/sec] (mean)
Time per request:       63.141 [ms] (mean)
Time per request:       0.631 [ms] (mean, across all concurrent requests)
Transfer rate:          202.61 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.6      0      10
Processing:    19   63  23.0     56     224
Waiting:       11   58  22.0     52     215
Total:         28   63  23.0     57     224

Percentage of the requests served within a certain time (ms)
  50%     57
  66%     60
  75%     65
  80%     66
  90%     71
  95%     90
  98%    170
  99%    188
 100%    224 (longest request)
