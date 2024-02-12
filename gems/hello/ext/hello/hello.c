#include "runtime.h"
#include "ruby.h"
#include <stdio.h>

#define TAG_NONE 0

__attribute__((import_module("asyncify"), import_name("start_unwind"))) void
asyncify_start_unwind(void *buf);
#define asyncify_start_unwind(buf)                                             \
  do {                                                                         \
    extern void *rb_asyncify_unwind_buf;                                       \
    rb_asyncify_unwind_buf = (buf);                                            \
    asyncify_start_unwind((buf));                                              \
  } while (0)
__attribute__((import_module("asyncify"), import_name("stop_unwind"))) void
asyncify_stop_unwind(void);
#define asyncify_stop_unwind()                                                 \
  do {                                                                         \
    extern void *rb_asyncify_unwind_buf;                                       \
    rb_asyncify_unwind_buf = NULL;                                             \
    asyncify_stop_unwind();                                                    \
  } while (0)
__attribute__((import_module("asyncify"), import_name("start_rewind"))) void
asyncify_start_rewind(void *buf);
__attribute__((import_module("asyncify"), import_name("stop_rewind"))) void
asyncify_stop_rewind(void);

void *rb_wasm_handle_jmp_unwind(void);
void *rb_wasm_handle_scan_unwind(void);
void *rb_wasm_handle_fiber_unwind(void (**new_fiber_entry)(void *, void *),
                                  void **arg0, void **arg1,
                                  bool *is_new_fiber_started);
static bool rb_should_prohibit_rewind = false;

__attribute__((noreturn)) void
rb_wasm_throw_prohibit_rewind_exception(const char *c_msg, size_t msg_len) {
    fprintf(stderr, "Rewinding control frame under nested VM operation is prohibited since it may cause inconsistency in VM state.\n");
    fprintf(stderr, "The operation that caused the rewind is: %.*s\n", (int)msg_len, c_msg);
    abort();
}

#define RB_WASM_CHECK_REWIND_PROHIBITED(msg)                                   \
  /*                                                                           \
    If the unwond source and rewinding destination are same, it's acceptable   \
    to rewind even under nested VM operations.                                 \
   */                                                                          \
  if (rb_should_prohibit_rewind &&                                             \
      (asyncify_buf != asyncify_unwound_buf || fiber_entry_point)) {           \
    rb_wasm_throw_prohibit_rewind_exception(msg, sizeof(msg) - 1);             \
  }

#define RB_WASM_LIB_RT(MAIN_ENTRY)                                             \
  {                                                                            \
                                                                               \
    void *arg0 = NULL, *arg1 = NULL;                                           \
    void (*fiber_entry_point)(void *, void *) = NULL;                          \
                                                                               \
    while (1) {                                                                \
      if (fiber_entry_point) {                                                 \
        fiber_entry_point(arg0, arg1);                                         \
      } else {                                                                 \
        MAIN_ENTRY;                                                            \
      }                                                                        \
                                                                               \
      bool new_fiber_started = false;                                          \
      void *asyncify_buf = NULL;                                               \
      extern void *rb_asyncify_unwind_buf;                                     \
      void *asyncify_unwound_buf = rb_asyncify_unwind_buf;                     \
      if (asyncify_unwound_buf == NULL)                                        \
        break;                                                                 \
      asyncify_stop_unwind();                                                  \
                                                                               \
      if ((asyncify_buf = rb_wasm_handle_jmp_unwind()) != NULL) {              \
        RB_WASM_CHECK_REWIND_PROHIBITED("rb_wasm_handle_jmp_unwind")           \
        asyncify_start_rewind(asyncify_buf);                                   \
        continue;                                                              \
      }                                                                        \
      if ((asyncify_buf = rb_wasm_handle_scan_unwind()) != NULL) {             \
        RB_WASM_CHECK_REWIND_PROHIBITED("rb_wasm_handle_scan_unwind")          \
        asyncify_start_rewind(asyncify_buf);                                   \
        continue;                                                              \
      }                                                                        \
                                                                               \
      asyncify_buf = rb_wasm_handle_fiber_unwind(&fiber_entry_point, &arg0,    \
                                                 &arg1, &new_fiber_started);   \
      if (asyncify_buf) {                                                      \
        RB_WASM_CHECK_REWIND_PROHIBITED("rb_wasm_handle_fiber_unwind")         \
        asyncify_start_rewind(asyncify_buf);                                   \
        continue;                                                              \
      } else if (new_fiber_started) {                                          \
        RB_WASM_CHECK_REWIND_PROHIBITED(                                       \
            "rb_wasm_handle_fiber_unwind but new fiber");                      \
        continue;                                                              \
      }                                                                        \
                                                                               \
      break;                                                                   \
    }                                                                          \
  }

static VALUE rb_eval_string_value_protect_thunk(VALUE str) {
  const ID id_eval = rb_intern("eval");
  VALUE binding = rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING"));
  const VALUE file = rb_utf8_str_new("eval", 4);
  VALUE args[3] = {str, binding, file};
  return rb_funcallv(rb_mKernel, id_eval, 3, args);
}

static VALUE rb_eval_string_value_protect(VALUE str, int *pstate) {
  return rb_protect(rb_eval_string_value_protect_thunk, str, pstate);
}

__attribute__((noinline))
static void rb_runtime_run(void *iseq) {
    ruby_run_node(iseq);
}

/// The exported entry point of this Component, defined by `gems/hello/wit/hello.wit`
void runtime_run() {
    extern void __wasm_call_ctors(void);
    __wasm_call_ctors();
    RUBY_INIT_STACK;
    RB_WASM_LIB_RT(ruby_init());
    const char *argv[] = { "ruby", "/src/main.rb", NULL };
    int argc = sizeof(argv) / sizeof(argv[0]) - 1;
    RB_WASM_LIB_RT(ruby_sysinit(&argc, (char ***)&argv));
    void *iseq;
    RB_WASM_LIB_RT(iseq = ruby_options(argc, (char **)argv));

    RB_WASM_LIB_RT(rb_runtime_run(iseq));
}

static inline void rstring_to_cabi_string(VALUE rstr, runtime_string_t *cabi_str) {
  cabi_str->len = RSTRING_LEN(rstr);
  cabi_str->ptr = xmalloc(cabi_str->len);
  memcpy(cabi_str->ptr, RSTRING_PTR(rstr), cabi_str->len);
}

static VALUE rb_runtime_print(VALUE self, VALUE str) {
    str = rb_obj_as_string(str);
    runtime_string_t cabi_string;
    rstring_to_cabi_string(str, &cabi_string);
    runtime_print(&cabi_string);

    return Qnil;
}

VALUE rb_mHello;

void
Init_hello(void)
{
  rb_mHello = rb_define_module("Hello");
  rb_define_module_function(rb_mHello, "runtime_print", rb_runtime_print, 1);
}
