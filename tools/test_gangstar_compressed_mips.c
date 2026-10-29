#include "compressed_mip_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    for (unsigned base = 1; base <= 4096; base <<= 1) {
        unsigned w = base;
        for (int level = 0;; ++level) {
            assert(vgl_compressed_mip_matches(level, base, base, w, w));
            if (w == 1) {
                assert(!vgl_compressed_mip_matches(level + 1, base, base, 1, 1));
                break;
            }
            w >>= 1;
        }
    }
    // Crash reproduction: 2048 source mip uploads following an unsuccessful
    // base replacement of a 512 GPU texture. These used to overwrite its heap.
    for (int level = 1; level <= 11; ++level) {
        unsigned w = 2048u >> level;
        assert(!vgl_compressed_mip_matches(level, 512, 512, w, w));
    }
    assert(vgl_compressed_mip_matches(9, 512, 1024, 1, 2));
    assert(vgl_compressed_mip_matches(10, 512, 1024, 1, 1));
    assert(!vgl_compressed_mip_matches(2, 512, 1024, 256, 128));
    assert(!vgl_compressed_mip_matches(-1, 512, 512, 512, 512));
    assert(!vgl_compressed_mip_matches(1, 0, 512, 1, 256));
    assert(!vgl_compressed_mip_matches(1, 512, 512, 0, 256));
    puts("Compressed mips: complete chains, rectangular tails and crash layout mismatch rejected");
}
