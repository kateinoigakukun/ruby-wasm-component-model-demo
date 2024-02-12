# frozen_string_literal: true

require "mkmf"

$objs = ["hello.o", "runtime.o", "#$srcdir/runtime_component_type.o"]
create_makefile("hello/hello")
