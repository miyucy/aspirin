# -*- coding: utf-8 -*-
require "#{File.dirname __FILE__}/../ext/aspirin"

class HelloApp
  def call(env)
    # env.keys.sort.each do |k|
    #   v = env[k]
    #   $stderr.puts "#{k.inspect} => #{v.inspect}"
    # end
    [200, {"Content-Type" => "text/plain"}, ["Hello, Rack"]]
  end
end
Rack::Handler::Aspirin.run HelloApp.new
__END__
∴ curl "localhost:9292/path/to/file?arg1=1&arg2=2"
∴ ab -n 10000 -c 100 http://127.0.0.1:9292/ 2>/dev/null
This is ApacheBench, Version 2.3 <$Revision: 655654 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 127.0.0.1 (be patient)


Server Software:
Server Hostname:        127.0.0.1
Server Port:            9292

Document Path:          /
Document Length:        11 bytes

Concurrency Level:      100
Time taken for tests:   3.184 seconds
Complete requests:      10000
Failed requests:        0
Write errors:           0
Total transferred:      750000 bytes
HTML transferred:       110000 bytes
Requests per second:    3141.16 [#/sec] (mean)
Time per request:       31.835 [ms] (mean)
Time per request:       0.318 [ms] (mean, across all concurrent requests)
Transfer rate:          230.07 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.7      0       9
Processing:     5   32   3.1     31      43
Waiting:        5   31   3.1     31      43
Total:         13   32   2.8     31      43

Percentage of the requests served within a certain time (ms)
  50%     31
  66%     32
  75%     33
  80%     33
  90%     35
  95%     36
  98%     40
  99%     41
 100%     43 (longest request)
