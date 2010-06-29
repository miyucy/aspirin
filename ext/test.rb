require "pp"
require "#{File.dirname __FILE__}/aspirin"

class HelloApp
  def call(env)
    # env.keys.sort.each do |k|
    #   v = env[k]
    #   puts "#{k.inspect} => #{v.inspect}"
    # end
    [200, {"Content-Type" => "text/plain"}, ["Hello, Rack"]]
  end
end
# srv = Aspirin::Server.new(HelloApp.new, nil)
# srv.start
Rack::Handler::Aspirin.run HelloApp.new
__END__
aspirin
"GATEWAY_INTERFACE" => "CGI/1.2"
"HTTP_ACCEPT" => "*/*"
"HTTP_HOST" => "localhost:9292"
"HTTP_USER_AGENT" => "curl/7.19.7 (i486-pc-linux-gnu) libcurl/7.19.7 OpenSSL/0.9.8k zlib/1.2.3.3 libidn/1.15"
"HTTP_VERSION" => "HTTP/1."
"PATH_INFO" => "/path/to/file"
"QUERY_STRING" => "arg1=1&arg2=2"
"REMOTE_ADDR" => "127.0.0.1"
"REQUEST_METHOD" => "GET"
"REQUEST_PATH" => "/path/to/file"
"REQUEST_URI" => "/path/to/file?arg1=1&arg2=2"
"SCRIPT_NAME" => ""
"SERVER_NAME" => "0.0.0.0"
"SERVER_PORT" => "9292"
"SERVER_PROTOCOL" => "HTTP/1.1"
"rack.errors" => #<IO:0xb776a554>
"rack.input" => #<StringIO:0xb773c2f8>
"rack.multiprocess" => false
"rack.multithread" => false
"rack.run_once" => false
"rack.url_scheme" => "http"
"rack.version" => [1, 1]

mongrel
"GATEWAY_INTERFACE" => "CGI/1.2"
"HTTP_ACCEPT" => "*/*"
"HTTP_HOST" => "localhost:8080"
"HTTP_USER_AGENT" => "curl/7.19.7 (i486-pc-linux-gnu) libcurl/7.19.7 OpenSSL/0.9.8k zlib/1.2.3.3 libidn/1.15"
"HTTP_VERSION" => "HTTP/1.1"
"PATH_INFO" => "/"
"QUERY_STRING" => ""
"REMOTE_ADDR" => "127.0.0.1"
"REQUEST_METHOD" => "GET"
"REQUEST_PATH" => "/"
"REQUEST_URI" => "/"
"SCRIPT_NAME" => ""
"SERVER_NAME" => "localhost"
"SERVER_PORT" => "8080"
"SERVER_PROTOCOL" => "HTTP/1.1"
"SERVER_SOFTWARE" => "Mongrel 1.1.5"
"rack.errors" => #<IO:0xb775f550>
"rack.input" => #<StringIO:0xb7032a0c>
"rack.multiprocess" => false
"rack.multithread" => true
"rack.run_once" => false
"rack.url_scheme" => "http"
"rack.version" => [1, 1]

Server Software:
Server Hostname:        127.0.0.1
Server Port:            9292

Document Path:          /
Document Length:        11 bytes

Concurrency Level:      10
Time taken for tests:   0.340 seconds
Complete requests:      1000
Failed requests:        0
Write errors:           0
Total transferred:      75000 bytes
HTML transferred:       11000 bytes
Requests per second:    2944.81 [#/sec] (mean)
Time per request:       3.396 [ms] (mean)
Time per request:       0.340 [ms] (mean, across all concurrent requests)
Transfer rate:          215.68 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    1   1.2      1       8
Processing:     0    2   1.1      2       8
Waiting:        0    2   1.3      1       6
Total:          1    3   1.2      3       9

Percentage of the requests served within a certain time (ms)
  50%      3
  66%      4
  75%      4
  80%      4
  90%      5
  95%      5
  98%      7
  99%      9
 100%      9 (longest request)
