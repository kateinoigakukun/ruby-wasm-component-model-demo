use wasmtime::component::*;
use wasmtime::{Config, Engine, Store};
use wasmtime_wasi::preview2::{WasiCtx, WasiCtxBuilder, WasiView};

bindgen!();

struct MyImports;

impl RuntimeImports for MyImports {
    fn print(&mut self, message: String) -> wasmtime::Result<()> {
        println!("HOST: {}", message);
        Ok(())
    }
}

struct AppCtx(MyImports, ResourceTable, WasiCtx);

impl WasiView for AppCtx {
    fn table(&self) -> &ResourceTable {
        &self.1
    }
    fn table_mut(&mut self) -> &mut ResourceTable {
        &mut self.1
    }
    fn ctx(&self) -> &WasiCtx {
        &self.2
    }
    fn ctx_mut(&mut self) -> &mut WasiCtx {
        &mut self.2
    }
}

fn main() -> wasmtime::Result<()> {
    let mut config = Config::new();
    config.cache_config_load_default()?;
    config.wasm_backtrace_details(wasmtime::WasmBacktraceDetails::Enable);
    config.wasm_component_model(true);
    let engine = Engine::new(&config)?;
    let component = Component::from_file(&engine, "./build/ruby.wasm")?;

    let mut linker = Linker::new(&engine);
    Runtime::add_to_linker(&mut linker, |ctx: &mut AppCtx| &mut ctx.0)?;
    wasmtime_wasi::preview2::command::sync::add_to_linker(&mut linker)?;

    let table = ResourceTable::new();
    let wasi_ctx = WasiCtxBuilder::new().inherit_stdio().args(&[""]).build();

    let mut store = Store::new(
        &engine,
        AppCtx(
            MyImports,
            table,
            wasi_ctx,
        )
    );
    let (bindings, _) = Runtime::instantiate(&mut store, &component, &linker)?;

    bindings.call_run(&mut store)?;
    Ok(())
}
