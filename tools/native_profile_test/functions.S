    .section .text.rio_test,"ax",%progbits
    .arm
    .align 12
    .global rio_test_begin
rio_test_begin:
    .global tf_sum6
    .type tf_sum6,%function
tf_sum6:                    @ (a,b,c,d,e,f) -> lo=a+..+f+1000, hi=0x55
    push {r4, r5, lr}
    ldr r4, 1f              @ 2nd instruction: positive PC literal
    add r0, r0, r1
    add r0, r0, r2
    add r0, r0, r3
    ldr r5, [sp, #12]
    add r0, r0, r5
    ldr r5, [sp, #16]
    add r0, r0, r5
    add r0, r0, r4
    mov r1, #0x55
    pop {r4, r5, pc}
1:  .word 1000
2:  .word 77
    .global tf_neg
    .type tf_neg,%function
tf_neg:                     @ a -> a+77 ; 1st instruction: negative PC literal
    ldr r2, 2b
    push {r4, lr}
    add r0, r0, r2
    pop {r4, pc}
    .global tf_outer
    .type tf_outer,%function
tf_outer:                   @ a -> (a+77)+2+3+4+5+6+1000, nested hooked calls with stack args
    push {r4, lr}
    mov r4, r0
    bl tf_neg
    mov r4, r0
    sub sp, sp, #8
    mov r0, #5
    str r0, [sp]
    mov r0, #6
    str r0, [sp, #4]
    mov r0, r4
    mov r1, #2
    mov r2, #3
    mov r3, #4
    bl tf_sum6
    add sp, sp, #8
    pop {r4, pc}
    .global tf_fact
    .type tf_fact,%function
tf_fact:                    @ n -> n! mod 2^32, recursive through its own hook
    push {r4, lr}
    mov r4, r0
    cmp r4, #1
    movle r0, #1
    pople {r4, pc}
    sub r0, r4, #1
    bl tf_fact
    mul r0, r4, r0
    pop {r4, pc}
    .global rio_test_end
rio_test_end:
    .align 12
    .section .note.GNU-stack,"",%progbits
