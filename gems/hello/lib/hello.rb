# frozen_string_literal: true

require "hello/hello.so"
require_relative "hello/version"

module Hello
  class Error < StandardError; end
end
