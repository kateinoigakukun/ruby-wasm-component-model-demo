build/wasi_snapshot_preview1.command.wasm:
	curl -L -o $@ https://github.com/bytecodealliance/wasmtime/releases/download/v17.0.1/wasi_snapshot_preview1.command.wasm

build/ruby.core.wasm:
	bundle exec rbwasm build -o build/ruby.core.wasm

build/ruby.wasm: build/ruby.core.wasm build/wasi_snapshot_preview1.command.wasm src/main.rb
	bundle exec rbwasm pack build/ruby.core.wasm --dir ./src::/src -o build/ruby.core.wasm
	wasm-tools component new build/ruby.core.wasm --adapt build/wasi_snapshot_preview1.command.wasm --output build/ruby.wasm

run:
	cargo run --release --manifest-path ./host/Cargo.toml
