#include "gangstar_startup_trampoline.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct { uint32_t r[16], memory[65536 / 4], zero; uint64_t d[32]; } Machine;
// Small independent interpreter for the allowed prologue instructions, used
// to compare original register/stack effects with relocated execution.
static void step(Machine *m) {
    uint32_t pc = m->r[15], word = m->memory[pc / 4];
    unsigned rd = (word >> 12) & 15;
    if (word == 0xe51ff004) { m->r[15] = m->memory[(pc + 4) / 4]; return; }
    if ((word & 0xffff0000) == 0xe59f0000) m->r[rd] = m->memory[(pc + 8 + (word & 4095)) / 4];
    else if ((word & 0xffff0000) == 0xe92d0000) {
        unsigned count = 0;
        for (unsigned i = 0; i < 16; ++i) count += (word >> i) & 1;
        uint32_t address = m->r[13] - 4 * count;
        for (unsigned i = 0; i < 16; ++i) if (word & (1u << i)) { m->memory[address / 4] = m->r[i]; address += 4; }
        m->r[13] -= 4 * count;
    } else if(word==0xed2d8b02) {
        m->r[13]-=8;
        m->memory[m->r[13]/4]=(uint32_t)m->d[8];
        m->memory[m->r[13]/4+1]=(uint32_t)(m->d[8]>>32);
    } else if ((word == 0xe2514000 || word == 0xe2513000)) { m->r[rd] = m->r[1]; m->zero = m->r[rd] == 0; }
    else if (word == 0xe3530000) m->zero = m->r[3] == 0;
    else if ((word & 0xfffff000) == 0xe24dd000) m->r[13] -= word & 255;
    else if ((word & 0xfff00000) == 0xe5d00000) {
        unsigned address = m->r[(word >> 16) & 15] + (word & 4095);
        m->r[rd] = (m->memory[address / 4] >> ((address & 3) * 8)) & 255;
    }
    else if ((word & 0xfff00000) == 0xe5900000) m->r[rd] = m->memory[(m->r[(word >> 16) & 15] + (word & 4095)) / 4];
    else if ((word & 0xffff0ff0) == 0xe1a00000) m->r[rd] = m->r[word & 15];
    else { assert((word & 0xffff0f00) == 0xe3a00000); m->r[rd] = word & 255; }
    m->r[15] = pc + 4;
}
int main(void) {
    const uint32_t prologues[][2] = {
        {0xe59f13e0, 0xe3a02000}, {0xe59f3398, 0xe92d4ff0},
        {0xe92d4ff0, 0xe1a07000}, {0xe92d45f0, 0xe59f818c},
        {0xe59f15b4, 0xe92d4ff0}, {0xe92d47f0, 0xe59f60fc},
        {0xe92d4ff0, 0xe59f918c}, {0xe92d4ff0, 0xe59f6360},
        {0xe92d4ff0, 0xe59030c4}, {0xe92d40f0, 0xe24dd014},
        {0xe92d45f0, 0xe24dd02c}, {0xe92d4ff0, 0xe24dd02c}, {0xe59f3048, 0xe92d4010},
        {0xe92d4ff0, 0xe59f415c}, {0xe92d4ff0, 0xe59fa1f8},
        {0xe92d4ff0, 0xe1a06001}, {0xe92d4070, 0xe1a05001}, {0xe92d4ff0, 0xe2514000},
        {0xe92d4ff0, 0xe1a06000}, {0xe92d40f0, 0xe24dd00c}, {0xe92d41f0, 0xe5913008},
        {0xe92d47f0, 0xe24dd098}, {0xe92d4ff0, 0xe59f4344},
        {0xe92d4ff0, 0xe59f449c}, {0xe92d41f0, 0xe59f51c0},
        {0xe92d4ff0, 0xe59f4dcc}, {0xe92d47f0, 0xe1a07000}, {0xe92d4070, 0xe1a05000},
        {0xe92d4070, 0xe5d03038}, {0xe92d4010, 0xe5913000},
        {0xe92d40f0, 0xe2513000}, {0xe59fc0a8, 0xe3a01000}, {0xe92d47f0, 0xe590a02c}, {0xe92d4ff0, 0xe590507c}, {0xe92d41f0, 0xe1a06000}, {0xe59f3040, 0xe3a02003}, {0xe92d47f0, 0xe3530000}, {0xe92d4ff0, 0xe1a06002}, {0xe92d4ff0, 0xe59f5978},
        {0xe92d41f0,0xed2d8b02},{0xe92d45f0,0xe5903118},
        {0xe92d4070,0xe59030f0},{0xe92d4010,0xe5903110},
        {0xe92d47f0,0xe1a04000},{0xe92d4010,0xe5903024},
        {0xe92d4070,0xe1a04000},{0xe92d4070,0xe1a04000},
        {0xe92d4ff0,0xe24dd06c},{0xe92d4ff0,0xe5902008},{0xe92d41f0,0xe1a05001}
    };
    static Machine original, relocated;
    for (unsigned scenario = 0; scenario < 2; ++scenario)
    for (unsigned p = 0; p < sizeof(prologues) / sizeof(*prologues); ++p) {
        memset(&original, 0, sizeof(original));
        for (unsigned i = 0; i < 15; ++i) original.r[i] = 0x12340000 + i;
        original.d[8]=UINT64_C(0xfedcba9876543210);
        original.r[13] = 0xf000; original.r[15] = 0x1000;
        original.r[0] = 0x6000; original.memory[(0x6000 + 196) / 4] = 0xabcdef12;
        if (scenario) { original.r[1] = 0; original.r[3] = 0; }
        if (prologues[p][1] == 0xe5913008 || prologues[p][1] == 0xe5913000) {
            original.r[1] = 0x7000; original.memory[0x7000 / 4] = 0x87654321; original.memory[0x7008 / 4] = 0xabcdef34;
        }
        original.memory[0x6038 / 4] = 0x78563412; original.memory[0x602c / 4] = 0xdeadb007;
        uint32_t literals[2] = {0xabcd0001, 0xabcd0002}, code[6];
        memcpy(&original.memory[0x1000 / 4], prologues[p], 8);
        for (unsigned i = 0; i < 2; ++i)
            if ((prologues[p][i] & 0xffff0000) == 0xe59f0000)
                original.memory[(0x1000 + i * 4 + 8 + (prologues[p][i] & 4095)) / 4] = literals[i];
        relocated = original; relocated.r[15] = 0x3000;
        assert(rio_startup_trampoline(code, 0x1000, prologues[p], literals));
        memcpy(&relocated.memory[0x3000 / 4], code, sizeof(code));
        step(&original); step(&original);
        for (unsigned i = 0; i < 3; ++i) step(&relocated);
        assert(!memcmp(original.r, relocated.r, sizeof(original.r)));
        assert(original.zero == relocated.zero);
        assert(!memcmp(original.d,relocated.d,sizeof(original.d)));
        assert(!memcmp(&original.memory[0xee00 / 4], &relocated.memory[0xee00 / 4], 512));
    }
    uint32_t code[6], bad[] = {0xeb000000, 0xe3a02000}, literal[] = {0, 0};
    assert(!rio_startup_trampoline(code, 0x1000, bad, literal));
    bad[0] = 0xe59ff000; assert(!rio_startup_trampoline(code, 0x1000, bad, literal));
    puts("Startup trampolines: all fifty prologues preserve registers/flags/stack/literals and return PC; unsupported instructions rejected");
}
