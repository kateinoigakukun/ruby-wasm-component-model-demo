require "/bundle/setup"
require "hello"

puts "Hello, world! #{Hello::VERSION}"
Hello.runtime_print "Hello, host world!"
