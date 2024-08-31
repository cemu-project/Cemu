.section .text

.global recompiler_fres

asmFresLookupTable:
    .word 0x07ff800, 0x03e1
    .word 0x0783800, 0x03a7
    .word 0x070ea00, 0x0371
    .word 0x06a0800, 0x0340
    .word 0x0638800, 0x0313
    .word 0x05d6200, 0x02ea
    .word 0x0579000, 0x02c4
    .word 0x0520800, 0x02a0
    .word 0x04cc800, 0x027f
    .word 0x047ca00, 0x0261
    .word 0x0430800, 0x0245
    .word 0x03e8000, 0x022a
    .word 0x03a2c00, 0x0212
    .word 0x0360800, 0x01fb
    .word 0x0321400, 0x01e5
    .word 0x02e4a00, 0x01d1
    .word 0x02aa800, 0x01be
    .word 0x0272c00, 0x01ac
    .word 0x023d600, 0x019b
    .word 0x0209e00, 0x018b
    .word 0x01d8800, 0x017c
    .word 0x01a9000, 0x016e
    .word 0x017ae00, 0x015b
    .word 0x014f800, 0x015b
    .word 0x0124400, 0x0143
    .word 0x00fbe00, 0x0143
    .word 0x00d3800, 0x012d
    .word 0x00ade00, 0x012d
    .word 0x0088400, 0x011a
    .word 0x0065000, 0x011a
    .word 0x0041c00, 0x0108
    .word 0x0020c00, 0x0106

recompiler_fres:
    sub     sp, sp, #48
    stp     x0, x1, [sp]
    fmov    x0, d31
    ubfx    x1, x0, #52, #11
    cmp     w1, #2047
    b.eq    fres_nan_or_inf
    cbnz    w1, fres_lookup 
    orr     x0, x0, #0x7ff0000000000000
    fmov    d31, x0
    ldp     x0, x1, [sp]
    add     sp, sp, #48
    ret

fres_nan_or_inf:
    stp     q0, q1, [sp, #16]
    movi    v0.2d, #0xffffffffffffffff
    movi    d1, #0000000000000000
    tst     x0, #0xfffffffffffff
    fneg    v0.2d, v0.2d
    bsl     v0.16b, v1.16b, v0.16b
    fcsel   d31, d1, d31, eq
    ldp     q0, q1, [sp, #16]
    add     sp, sp, #48
    ret

fres_lookup:
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    ubfx    x2, x0, #47, #5
    adrp    x3, asmFresLookupTable
    add     x3, x3, :lo12:asmFresLookupTable
    ubfx    x4, x0, #37, #10
    mov     w5, #1
    and     x0, x0, #0x8000000000000000
    add     x2, x3, x2, lsl #3
    ldp     w2, w3, [x2]
    madd    w3, w3, w4, w5
    mov     w4, #2045
    sub     w1, w4, w1
    orr     x0, x0, x1, lsl #52
    sub     w2, w2, w3, lsr #1
    add     x0, x0, x2, lsl #29
    fmov    d31, x0
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x0, x1, [sp]
    add     sp, sp, #48
    ret



asmFrsqrteLookupTable:
    .word 0x01a7e800, 0x0568
    .word 0x017cb800, 0x04f3
    .word 0x01552800, 0x048d
    .word 0x0130c000, 0x0435
    .word 0x010f2000, 0x03e7
    .word 0x0eff000, 0x03a2
    .word 0x0d2e000, 0x0365
    .word 0x0b7c000, 0x032e
    .word 0x09e5000, 0x02fc
    .word 0x0867000, 0x02d0
    .word 0x06ff000, 0x02a8
    .word 0x05ab800, 0x0283
    .word 0x046a000, 0x0261
    .word 0x0339800, 0x0243
    .word 0x0218800, 0x0226
    .word 0x0105800, 0x020b
    .word 0x03ffa000, 0x07a4
    .word 0x03c29000, 0x0700
    .word 0x038aa000, 0x0670
    .word 0x03572000, 0x05f2
    .word 0x03279000, 0x0584
    .word 0x02fb7000, 0x0524
    .word 0x02d26000, 0x04cc
    .word 0x02ac0000, 0x047e
    .word 0x02881000, 0x043a
    .word 0x02665000, 0x03fa
    .word 0x02468000, 0x03c2
    .word 0x02287000, 0x038e
    .word 0x020c1000, 0x035e
    .word 0x01f12000, 0x0332
    .word 0x01d79000, 0x030a
    .word 0x01bf4000, 0x02e6

.global recompiler_frsqrte

recompiler_frsqrte:
    sub     sp, sp, #48
    stp     x0, x1, [sp]
    fcmp    d31, #0.0
    fmov    x0, d31
    b.ne    frsqrte_not_zero
    // result is inf or -inf
    orr     x0, x0, #0x7ff0000000000000
    fmov    d31, x0
    ldp     x0, x1, [sp]
    add     sp, sp, #48
    ret

frsqrte_not_zero:
    stp     x2, x3, [sp, #16]
    lsr     x1, x0, #52
    mov     w2, #2047
    bics    wzr, w2, w1
    // branch to frsqrte_lookup if not NaN or Inf
    b.ne    frsqrte_lookup
    // branch to frsqrte_inf if not NaN
    tst     x0, #0xfffffffffffff
    b.eq    frsqrte_inf
    // result is NaN with same sign and same mantissa 
    ldp     x0, x1, [sp]
    ldp     x2, x3, [sp, #16]
    add     sp, sp, #48
    ret

frsqrte_inf:
    // if -INF result is +NaN (#9221120237041090560)
    // if +INF result is +0.0
    str     q0, [sp, #32]
    movi    d31, #0000000000000000
    mov     x1, #9221120237041090560
    cmp     x0, #0
    fmov    d0, x1
    fcsel   d31, d0, d31, lt
    ldp     x0, x1, [sp]
    ldp     x2, x3, [sp, #16]
    ldr     q0, [sp, #32]
    add     sp, sp, #48
    ret

frsqrte_lookup:
    tbnz    x0, #63, frsqrte_negative_input
    ubfx    x2, x0, #48, #5
    adrp    x3, asmFrsqrteLookupTable
    add     x3, x3, :lo12:asmFrsqrteLookupTable
    ubfx    x0, x0, #37, #11
    add     x2, x3, x2, lsl #3
    ldp     w2, w3, [x2]
    msub    w0, w3, w0, w2
    mov     w2, #7171
    add     w1, w1, w2
    mov     w2, #1023
    sub     w1, w2, w1, lsr #1
    sbfiz   x0, x0, #26, #32
    add     x0, x0, x1, lsl #52
    fmov    d31, x0
    ldp     x0, x1, [sp]
    ldp     x2, x3, [sp, #16]
    add     sp, sp, #48
    ret

frsqrte_negative_input:
    mov     x0, #9221120237041090560
    fmov    d31, x0
    ldp     x0, x1, [sp]
    ldp     x2, x3, [sp, #16]
    add     sp, sp, #48
    ret