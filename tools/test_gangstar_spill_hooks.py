"""vitaGL must never hand a reserved texture block to newlib, and must use one only last."""
from pathlib import Path
mem = Path('lib/vitagl/source/utils/mem_utils.c').read_text()
def body(text, start):
    a = text.index(start)
    return text[a:text.index('\n}\n', a)]
free = body(mem, 'void vgl_free(void *ptr) {')
assert free.index('gangstar_spill_put(ptr)') < free.index('__real_free(ptr)')
usable = body(mem, 'size_t vgl_malloc_usable_size(void *ptr) {')
assert 'return spill ? spill : malloc_usable_size(ptr);' in usable
realloc = body(mem, 'void *vgl_realloc(void *ptr, size_t size) {')
assert realloc.index('return size <= spill ? ptr : NULL;') < realloc.index('__real_realloc(ptr, size)')
gpu = Path('lib/vitagl/source/utils/gpu_utils.c').read_text()
alloc = body(gpu, 'void *gpu_alloc_mapped_aligned_for_gpu(size_t alignment, size_t size) {')
assert alloc.index('gangstar_memory_relieve(size)') < alloc.index('unsafe_for_gpu(alignment, size)') < alloc.index('gangstar_spill_get(alignment, size)')
assert gpu.count('gangstar_spill_get(alignment, size)') == 1
start = Path('source/main.c').read_text()
assert start.index('gangstar_spill_init();') < start.index('int result = gangstar_main();')
print('Spill hooks: free, realloc, usable size and last-resort order verified')
