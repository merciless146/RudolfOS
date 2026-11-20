/* Rudolf OS: setjmp/longjmp in Emu's WebAssembly cores (clang 18, -mllvm -wasm-enable-sjlj). clang 18 calls these
   helpers by their LLVM 18 names; the WASI libraries (wasi-sysroot 22's libsetjmp) have the throwing side
   (__wasm_longjmp, __c_longjmp) under the newer names only. As in LLVM 18's own runtime (and Emscripten's).
   GPL-2.0-or-later. */
#include <stdint.h>
#include <stdlib.h>
typedef struct { uint32_t id, label; } TableEntry;
static uint32_t temp_ret, setjmp_id;
uint32_t getTempRet0(void) { return temp_ret; }
void setTempRet0(uint32_t v) { temp_ret = v; }
TableEntry *saveSetjmp(uint32_t *env, uint32_t label, TableEntry *table, uint32_t size)
{
	uint32_t id = ++setjmp_id; *env = id;
	for (uint32_t i = 0; i < size; i++) if (table[i].id == 0) { table[i].id = id; table[i].label = label; table[i + 1].id = 0; temp_ret = size; return table; }
	size *= 2; table = realloc(table, sizeof(TableEntry) * (size + 1)); table = saveSetjmp(env, label, table, size); temp_ret = size; return table;
}
uint32_t testSetjmp(uint32_t id, TableEntry *table, uint32_t size)
{
	(void)size; uint32_t c;
	for (uint32_t i = 0; (c = table[i].id) != 0; i++) if (c == id) return table[i].label;
	return 0;
}
