# ruby-wasm-component-model-demo

This is an example project to demonstrate the use of [`ruby.wasm`](https://github.com/ruby/ruby.wasm) and [Component Model](https://github.com/WebAssembly/component-model).

This project just demonstrates how to communicate between guest WebAssembly components and host runtime, but it is not a complete application.

## Prerequisites

```
$ cargo install wasm-tools --git https://github.com/bytecodealliance/wasm-tools --tag wasm-tools-1.0.58
$ cargo install wit-bindgen-cli --git https://github.com/bytecodealliance/wit-bindgen --tag wit-bindgen-cli-0.17.0
$ bundle install
```

## Build and Run

```
$ make run
Hello, world! 0.1.0
HOST: Hello, host world!
```

You can update the `src/main.rb` file and re-run the `make run` command to see the changes.

### Re-generate bindgen files

If you modify `gems/hello/wit`, you need to re-generate bindgen files, and re-build extension libraries as described in the next section.

```
$ wit-bindgen c --autodrop-borrows=yes --out-dir=./ext/hello/ ./wit/
```

### Re-build extension libraries

If you modify `gems/hello/ext`, you need to re-build the extension libraries.

```
$ bundle exec rbwasm build -o build/ruby.core.wasm --remake
$ make run
```

## File Structure

- `gems/hello`: A Ruby gem that is executed inside the WebAssembly guest component.
- `gems/hello/ext`: An extension library that is compiled to WebAssembly. Used to brige WebAssembly's import/export and Ruby API.
- `gems/hello/ext/{runtime.c,runtime.h,runtime_component_type.o}`: wit-bindgen's output files. These files are generated from `gems/hello/wit`.
- `gems/hello/wit`: A WebAssembly Interface Types (WIT) file that describes the interface of this component.
- `host`: A Rust project that runs the WebAssembly guest component while providing host functionalities.
- `src/main.rb`: The main entry point of the guest Ruby application. The path is specified in `gems/hello/ext/hello.c`.
- `Gemfile`: A list of Ruby gems that are used in the guest Ruby application. `gems/hello` is specified in this file. You can add more gems (if they are supported[^1]) to this file.


[^1]: Typically, gems that are written in pure Ruby are supported. Simple C extensions are also supported, but most of major C extensions are not supported yet.
