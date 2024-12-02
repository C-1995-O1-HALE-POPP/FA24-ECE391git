        .section	.text

        # Input:
        # - None
        #
        # Output:
        # - Not returned. If `main` returns 0, it will jump to `halt_success`
        #   If `main` returns a non-zero value, it will jump to `halt_failure`
        #
        # Description:
        # - This function is used to set up M mode to delegate interrupts and exceptions 
        # to supervisor S mode, granting S mode access to all physical memory, and configuring various status registers.
        # It also sets trap handler addresses and enables access to cycle, time, and instruction counters in S and U modes. 
        # Finally, it switches to supervisor mode and initializes the stack pointer for the main thread.
        #
        # Side effect:
        # - Modifies multiple control and status registers
        # - Switches to supervisor S Mode and sets up the stack in the main thread

        addi    t0, zero, -1
        csrw    medeleg, t0
        csrw    mideleg, t0
        csrw    pmpaddr0, t0
        csrsi   pmpcfg0, 0xf

        # Set trap handler address (defined in trapasm.s)

        la      t0, _mmode_trap_entry
        la      t1, _trap_entry_from_smode
        csrw    mtvec, t0
        csrw    stvec, t1

        li      t0, 1UL << 18
        csrs    mstatus, t0
        csrc    mstatus, t0
        csrs    sstatus, t0
        csrc    sstatus, t0

        # Enable access to cycle, time, and instret counters in S and U mode

        csrs    mcounteren, 7
        csrs    scounteren, 7

        # Switch to S mode

        li      t0, 0x1080 # bits to clear in mstatus (MPP=01,MPIE=0)
        li      t1, 0x0800 # bits to set in mstatus (MPP=01)
        csrc    mstatus, t0
        csrs    mstatus, t1
        la      t0, 1f
        csrw    mepc, t0
        mret
1:      

        # Set stack pointer. The main thread uses a statically-allocated stack
        # in the .data section.

        la	sp, _main_stack_anchor
        mv      fp, zero

        # If main returns 0, jump to halt_success, otherwise to halt_failure

        call    main
        bnez    a0, halt_failure
        j       halt_success

        .section        .data.stack, "wa", @progbits
        .balign		16
        
        .equ		MAIN_STACK_SIZE, 16384

        .global		_main_stack_lowest
        .type		_main_stack_lowest, @object
        .size		_main_stack_lowest, MAIN_STACK_SIZE

        .global		_main_stack_anchor
        .type		_main_stack_anchor, @object
        .size		_main_stack_anchor, 2*8

_main_stack_lowest:
        .fill   MAIN_STACK_SIZE, 1, 0xA5

_main_stack_anchor:
        .global main_thread # from thread.c
        .dword  main_thread
        .fill   8

        .end
