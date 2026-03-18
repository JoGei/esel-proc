/* crt0.S */
    /*.section .text.startup*/
    .section .init, "ax", @progbits
    .globl _start
    .type  _start, @function

_start:
    /* stack */
    la      sp, __stack_top

    /* copy .data from ROM load image to RAM */
    la      t0, __data_load_start
    la      t1, __data_start
    la      t2, __data_end
1:
    bgeu    t1, t2, 2f
    lw      a0, 0(t0)
    sw      a0, 0(t1)
    addi    t0, t0, 4
    addi    t1, t1, 4
    j       1b

    /* zero .bss */
2:
    la      t1, __bss_start
    la      t2, __bss_end
3:
    bgeu    t1, t2, 4f
    sw      zero, 0(t1)
    addi    t1, t1, 4
    j       3b

    /* call main() */
4:
    call    main

    /* if main returns, hang */
5:
    j       5b

    .size _start, . - _start