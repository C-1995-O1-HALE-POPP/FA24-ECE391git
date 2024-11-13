# mp1.s - Your solution goes here
#
        .section .data                    # Data section (optional for global data)
        .extern skyline_beacon            # Declare the external global variable 

x_dbg:  .word   0
y_dbg:  .word   0
offset_dbg:
        .word   0
pos_dbg:.word   0
blockNum_dbg:
        .word   0

        .global skyline_star_cnd          # current star number 
        .type   skyline_star_cnd, @object

        # const
        .equ    DEFAULT_STAR_DIA,  1      # default star diameter
        .equ    SIZE_OF_STAR,      8      # sizeof(skyline_star) = 8 without padding
        .equ    SIZE_OF_WINDOW,    16     # sizeof(skyline_window) = 16 without padding
        .equ    DEFAULT_BEACON_DIA,4      # default beacon diameter
        .equ    SIZE_OF_BEACON,    24     # sizeof(skyline_beacon) = 24 without padding
        .equ    SKYLINE_WIDTH,     640
        .equ    SKYLINE_HEIGHT,    480
        .equ    SKYLINE_STARS_MAX, 1000

        .section .text                    # Text section (optional for global functions)
        .global start_beacon
        .type   start_beacon, @function

        # extern void add_star(uint16_t x, uint16_t y, uint16_t color);
        .global add_star                  
        .type   add_star, @function

        # extern void add_star(uint16_t x, uint16_t y, uint16_t color);
        .global remove_star                  
        .type   remove_star, @function

        #extern void draw_star(uint16_t * fbuf, const struct skyline_star * star);
        .global draw_star                  
        .type   draw_star, @function

        # extern void skyline_init(void);
        .global skyline_init                  
        .type   skyline_init, @function

        # extern void add_window(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint16_t color);
        .global add_window                  
        .type   add_window, @function

        # extern void remove_window(uint16_t x, uint16_t y);
        .global remove_window                  
        .type   remove_window, @function

        # extern void draw_window(uint16_t * fbuf, const struct skyline_window * win);
        .global draw_window                  
        .type   draw_window, @function

        # extern void draw_beacon (uint16_t * fbuf, uint64_t t, const struct skyline_beacon * bcn);
        .global draw_beacon                  
        .type   draw_beacon, @function

        

start_beacon:

        la t0, skyline_beacon             # Load address of skyline_beacon into t0 (t0 is 64-bit)

        # Store the function arguments into the struct fields
        sd a0, 0(t0)                      # Store img (a0, 64-bit) at offset 0 (8 bytes)

        sh a1, 8(t0)                      # Store x (a1, 16-bit) at offset 8 (after img pointer)

        sh a2, 10(t0)                     # Store y (a2, 16-bit) at offset 10

        sb a3, 12(t0)                     # Store dia (a3, 8-bit) at offset 12

        sh a4, 14(t0)                     # Store period (a4, 16-bit) at offset 14

        sh a5, 16(t0)                     # Store ontime (a5, 16-bit) at offset 16

        ret                               # Return to caller


add_star:
        # extern void add_star(uint16_t x, uint16_t y, uint16_t color);
        addi sp, sp, -16
        sd   ra, 8(sp)
        sd   fp, 0(sp)
        addi fp, sp, 16

        # star out of canvas, skip;
        li   t0, SKYLINE_WIDTH
        bleu t0, a0, skip_add_star
        addi t0, zero, SKYLINE_HEIGHT
        bleu t0, a1, skip_add_star

        # in case of too much stars, skip; else 
        la   t2, skyline_star_cnt               # t2 -> counter
        lhu  t0, 0(t2)                          # t0 -> index
        addi t1, zero, SKYLINE_STARS_MAX        # t1 -> max
        beq  t0, t1, skip_add_star
        


        # calculate index and offset in star array
        la   t1, skyline_stars
        li   t6, SIZE_OF_STAR
        mul  t0, t0, t6                          
        add  t0, t1, t0                         # t0 -> offset
        
        # load parameters into array
        addi t1, zero, DEFAULT_STAR_DIA         # set t1 default diameter
        sh   a0, 0(t0)                          # a0 -> x
        sh   a1, 2(t0)                          # a1 -> y
        sb   t1, 4(t0)                          # t1 -> diameter
        sh   a2, 6(t0)                          # a2 -> color
        
        # increment counter
        la   t1, skyline_star_cnt
        lhu  t0, 0(t1)
        addi t0, t0, 1
        sh   t0, 0(t1)

    skip_add_star:
        ld   fp, 0(sp)
        ld   ra, 8(sp)
        addi sp, sp, 16
        ret
        

remove_star:
        # extern void remove_star(uint16_t x, uint16_t y);
        addi sp, sp, -16
        sd   ra, 8(sp)
        sd   fp, 0(sp)
        addi fp, sp, 16

        # load init address and loop
        la   t0, skyline_stars                  # t0 -> current mem addr
        la   t1, skyline_star_cnt               # t1 -> skyline_star_cnt
        lhu  t1, 0(t1)
        add  t2, zero, zero                     # t2 -> index

        # if_array empty, skip
        beqz t1, finish_remove_star

        # find a star at given position
    loop_find_remove_star:
        beq  t2, t1, finish_remove_star

        # if_not in position, skip
        lhu  t3, 0(t0)
        bne  t3, a0, not_find_remove_star
        lhu  t3, 2(t0)
        bne  t3, a1, not_find_remove_star

        # we find it!
        j    shift_star_array

        # increment address and index
    not_find_remove_star:
        addi t2, t2, 1
        addi t0, t0, SIZE_OF_STAR
        j loop_find_remove_star

    finish_remove_star:
        ld   fp, 0(sp)
        ld   ra, 8(sp)
        addi sp, sp, 16
        ret

    shift_star_array:
        # exchange with last one (if skyline_star_cnt >= 2)
        # and decrement `skyline_star_cnt`;
        # t0 -> current mem addr; t1 -> skyline_star_cnt;
        addi t2, zero, 2
        bltu t1, t2, finish_swap_remove_star    # swap, if skyline_star_cnt >= 2

        # get address of skyline_stars[-1]
        # t3 -> offset; t4 -> &(skyline_stars[-1]); t0 -> &(skyline_stars[i])
        addi t3, zero, SIZE_OF_STAR
        mul  t3, t3, t1
        addi t2, zero, SIZE_OF_STAR
        sub  t3, t3, t2                         # offset = SIZE*(cnt-1)
        la   t4, skyline_stars
        add  t4, t3, t4

        # swap x, y, dia, and color via t2; discard original values in i
        # t2 -> in -1
        csrrci x0,mstatus,8 # disable interrupts for m-mode
        lhu  t2, 0(t4)
        sh   t2, 0(t0)
        lhu  t2, 2(t4)
        sh   t2, 2(t0)
        lhu  t2, 4(t4)
        sh   t2, 4(t0)
        lhu  t2, 6(t4)
        sh   t2, 6(t0)
        csrrsi x0,mstatus,8 # re-enable interrupts for m-mode

    finish_swap_remove_star:
        # decrement `skyline_star_cnt`;
        addi t1, t1, -1
        la   t2, skyline_star_cnt
        sh   t1, 0(t2)
        j finish_remove_star

draw_star:
        # extern void draw_star(uint16_t * fbuf, const struct skyline_star * star);
        addi sp, sp, -16
        sd   ra, 8(sp)
        sd   fp, 0(sp)
        addi fp, sp, 16

        # check the address is valid (aligned and within index)
        # t0 -> offset
        la   t0, skyline_stars
        bltu a1, t0, finish_draw_star           # address is smaller, skip
        sub  t0, a1, t0
        addi t1, zero, SIZE_OF_STAR
        rem  t2, t0, t1
        bnez t2, finish_draw_star               # address is not aligned, skip
        div  t2, t0, t1
        la   t0, skyline_star_cnt
        lhu  t0, 0(t0)
        bleu t0, t2, finish_draw_star           # address index >= skyline_star_cnt, skip

        # now we can print,
        # first determine memory addr
        # t0 -> offset; t1 -> get values from struct
    draw_star_dbg:
        lhu  t1, 2(a1)                          
        addi t0, zero, SKYLINE_WIDTH            
        mul  t0, t0, t1                         # t0 -> y*640
        lhu  t1, 0(a1)
        add  t0, t1, t0                         # t0 -> y*640+x
        li   t5, SKYLINE_WIDTH
        li   t6, SKYLINE_HEIGHT
        mul  t5, t6, t5
        bgeu t0, t5, finish_draw_star
        addi t1, zero, 2                    
        add  t0, t0, t0                         # t0 -> 2*(y*640+x)
        add  t0, a0, t0                         # t0 -> device_addr + offset
        # then send color to fbuf[offset], via t1
        lhu  t1, 6(a1)
        sh   t1, 0(t0)   

    finish_draw_star:
        ld   fp, 0(sp)
        ld   ra, 8(sp)
        addi sp, sp, 16
        ret

add_window:
        # extern void add_window(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint16_t color);
        addi sp, sp, -16
        sd   ra, 8(sp)
        sd   fp, 0(sp)
        addi fp, sp, 16

        # star out of canvas, skip;
        addi t0, zero, SKYLINE_WIDTH
        bleu t0, a0, skip_add_window
        addi t0, zero, SKYLINE_HEIGHT
        bleu t0, a1, skip_add_window

        # allocate memory space
        # push parameters into stack
        addi sp, sp, -2
        sh   a4, 0(sp)
        addi sp, sp, -1
        sb   a3, 0(sp)
        addi sp, sp, -1
        sb   a2, 0(sp)
        addi sp, sp, -2
        sh   a1, 0(sp)
        addi sp, sp, -2
        sh   a0, 0(sp)
        addi sp, sp, -8
        sw   ra, 0(sp)
allocate_window:
        addi a0, zero, SIZE_OF_WINDOW
        call malloc
        beqz a0, allocate_window                # if allocation failure, retry
        addi t0, a0, 0                          # t0 = address of new_window

        # pop all arguments
        lw   ra, 0(sp)
        addi sp, sp, 8
        lhu  a0, 0(sp)
        addi sp, sp, 2
        lhu  a1, 0(sp)
        addi sp, sp, 2
        lbu  a2, 0(sp)
        addi sp, sp, 1
        lbu  a3, 0(sp)
        addi sp, sp, 1
        lhu  a4, 0(sp)
        addi sp, sp, 2

        # link with head and switch the head
        la   t2, skyline_win_list               # t2 -> skyline_win_list
        ld   t3, 0(t2)                          # t3 =  skyline_win_list
        sd   t3, 0(t0)                          # new_window->next = skyline_win_list
        sd   t0, 0(t2)                          # skyline_win_list = new_window

        # load parameters into array
        sh   a0, 8(t0)                          # a0 -> x
        sh   a1, 10(t0)                         # a1 -> y
        sb   a2, 12(t0)                         # a2 -> w
        sb   a3, 13(t0)                         # a3 -> h
        sh   a4, 14(t0)                         # a4 -> color
        
    skip_add_window:
        ld   fp, 0(sp)
        ld   ra, 8(sp)
        addi sp, sp, 16
        ret
        

remove_window:
        # extern void remove_window(uint16_t x, uint16_t y);
        addi sp, sp, -16
        sd   ra, 8(sp)
        sd   fp, 0(sp)
        addi fp, sp, 16

        # traverlse with the link until next == NULL
        # head and last are pointers!
        # t0 = head; t1 = last -> head (if we want remove first elem, set 0(t1) = head -> next)
        la   t1, skyline_win_list
        ld   t0, 0(t1)

        # while(head!=null):
        #   if_(head->x==x && head_y==y): delete this node and break
        #   else_ last = head; head=head->next
    loop_remove_window:
        beqz t0, finish_remove_window
        lhu  t2, 8(t0)                          # t2 = x
        bne  a0, t2, not_this_window
        lhu  t2, 10(t0)                         # t2 = y
        bne  a1, t2, not_this_window

        # link and delete
        # last -> next = head -> next or *(&skyline_win_list) = head -> next
        csrrci x0,mstatus,8 # re-enable interrupts for m-mode
        ld   t2, 0(t0)
        sd   t2, 0(t1)
        csrrsi x0,mstatus,8 # re-enable interrupts for m-mode

        # fill all parameters zero to prevent some magic overflow
        sd   zero, 0(t0)
        sh   zero, 8(t0)
        sh   zero, 10(t0)
        sb   zero, 12(t0)
        sb   zero, 13(t0)
        sh   zero, 14(t0)

        # free head
        # push a0, a1 into stack, then a0 = head
        addi sp, sp, -2
        sh   a1, 0(sp)
        addi sp, sp, -2
        sh   a0, 0(sp)
        addi sp, sp, -8
        sw   ra, 0(sp)
        addi a0, t0, 0
        # call function
        call free
        # pop back
        lw   ra, 0(sp)
        addi sp, sp, 8
        lh   a0, 0(sp)
        addi sp, sp, 2
        lh   a1, 0(sp)
        addi sp, sp, 2

    not_this_window:
        addi t1, t0, 0                          # last = head
        ld   t0, 0(t0)                          # head = head->next
        j    loop_remove_window

    finish_remove_window:
        ld   fp, 0(sp)
        ld   ra, 8(sp)
        addi sp, sp, 16
        ret


draw_window:
        # extern void draw_star(uint16_t * fbuf, const struct skyline_star * star);
        addi sp, sp, -8
        sd   ra, 0(sp)
        addi sp, sp, -8
        sd   fp, 0(sp)
        addi fp, sp, 16

        # iterate the linklist
        # head is pointer
        # t0 = a1 = head
        addi t0, a1, 0

        # draw_rectangle(fbuf, x, y, w, h, color);
        # a0 = fbuf
        lhu  a1, 8(t0)                          # a1 = x
        lhu  a2, 10(t0)                         # a2 = y
        lbu  a3, 12(t0)                         # a3 = w
        lbu  a4, 13(t0)                         # a4 = h
        lhu  a5, 14(t0)                         # a5 = color

        csrrci x0,mstatus,8
        add  t0, zero, zero             # t0 = w
        add  t1, zero, zero             # t1 = h
    loop_y_draw_rectangle:
        bgeu t1, a4, finish_draw_window
        add  t0, zero, zero             # t0 = w
    loop_x_draw_rectangle:
        bgeu t0, a3, end_loop_x_draw_rectangle

        # t2 = x coordinate; t3 = y coordinate
        add  t2, t0, a1
        add  t3, t1, a2

        # check if this pixel out of boundary
        li   t4, SKYLINE_WIDTH
        bleu t4, t2, skip_pixel_draw_rectangle
        li   t4, SKYLINE_HEIGHT
        bleu t4, t3, skip_pixel_draw_rectangle
                
        la   t6, x_dbg
        sh   t2, 0(t6)
        la   t6, y_dbg
        sh   t3, 0(t6)
        # get offset if available and set color
        addi t4, zero, SKYLINE_WIDTH
        mul  t4, t4, t3
        add  t4, t4, t2

        # prevent runtime crash?
        li   t5, SKYLINE_WIDTH
        li   t6, SKYLINE_HEIGHT
        mul  t5, t5, t6
        bgeu t4, t5, skip_pixel_draw_rectangle

        add  t2, t4, t4                 # t2 = 2*(x+640*y)
        la   t6, offset_dbg
        sh   t2, 0(t6)
        add  t2, a0, t2
        la   t6, pos_dbg
        sd   t2, 0(t6)

        sh   a5, 0(t2)                  # fbuf[t2] = color crash here in gdb

    skip_pixel_draw_rectangle:
        addi t0, t0, 1
        j loop_x_draw_rectangle
    end_loop_x_draw_rectangle:
        addi t1, t1, 1
        j    loop_y_draw_rectangle
    

    finish_draw_window:
        csrrsi x0,mstatus,8
        ld   fp, 0(sp)
        addi sp, sp, 8
        ld   ra, 0(sp)
        addi sp, sp, 8
        ret
    


draw_beacon:
        # extern void draw_beacon (uint16_t * fbuf, uint64_t t, const struct skyline_beacon * bcn);

        addi sp, sp, -16
        sd   ra, 8(sp)
        sd   fp, 0(sp)
        addi fp, sp, 16
        
        # t0 = period; t1 = ontime
        lhu  t0, 14(a2)
        lhu  t1, 16(a2)

        # on in before on time in one period
        rem  t0, a1, t0                 # t1 = t%period
        bgeu t0, t1, finish_draw_beacon   

        # t3 = pointer to color array, do not replace
        ld   t3, 0(a2)

        # iterate x and y coordinate
        li   t0, 0                      # t0 = x
        li   t1, 0                      # t1 = y
    y_loop_draw_beacon:
        lbu   t2, 12(a2)                # t2 = dia
        bgeu  t1, t2, finish_draw_beacon

        # initialize inner loop
        li   t0, 0                      # x = 0
    x_loop_draw_beacon:
        lbu   t2, 12(a2)                # t2 = dia
        bgeu  t0, t2, end_x_loop_draw_beacon
        # get color of this pixel in t2
        # t2 = index in color array = y*dia+x
        mul  t2, t1, t2
        add  t2, t0, t2
        li   t4, 2
        mul  t2, t4, t2                 # get actual offset in t2 by *= size
        add  t2, t3, t2                 # add original address
        lhu  t2, 0(t2)                  # get color value

        # get position of this pixel
        # t4 = x; t5 =y;
        lhu  t4, 8(a2)
        lhu  t5, 10(a2)
        add  t4, t4, t0
        add  t5, t5, t1

        # if_out of canvas, skip
        li   t6, SKYLINE_WIDTH
        bleu t6, t4, skip_pixel_draw_beacon
        li   t6, SKYLINE_HEIGHT
        bleu t6, t5, skip_pixel_draw_beacon

        # range test, push t0 into stack, t6 as temp
        addi sp, sp, -8
        sh   t0, 0(sp)

        li   t0, SKYLINE_WIDTH
        li   t6, SKYLINE_HEIGHT
        mul  t0, t0, t6

        # get offset in t6 = x+640*y
        addi t6, zero, SKYLINE_WIDTH
        mul  t6, t6, t5
        add  t6, t6, t4
        # range test cond
        bgeu t6, t0, skip_pixel_draw_beacon

        add  t4, t6, t6                     # *=2 as offset

        # access actual address (t4) and write
        add  t4, a0, t4
        sh   t2, 0(t4)

        # pop t0
        lhu  t0, 0(sp)
        addi sp, sp, 8

    skip_pixel_draw_beacon:
        addi t0, t0, 1
        j    x_loop_draw_beacon
        # inner loop

    end_x_loop_draw_beacon:
        addi t1, t1, 1
        j    y_loop_draw_beacon
        # outer loop

    finish_draw_beacon:
        ld   fp, 0(sp)
        ld   ra, 8(sp)
        addi sp, sp, 16
        ret

skyline_init:
        # extern void skyline_init(void);
        addi sp, sp, -16
        sd   ra, 8(sp)
        sd   fp, 0(sp)
        addi fp, sp, 16

        # skyline_star_cnt = 0;
        la   t0, skyline_star_cnt
        sh   zero, 0(t0)

        # skyline_win_list = NULL;
        la   t0, skyline_win_list
        sd   zero, 0(t0)
        
        # clear beacon settings 
        la   t0, skyline_beacon
        sd   zero, 0(t0)
        sh   zero, 8(t0)
        sh   zero, 10(t0)
        sh   zero, 12(t0)
        sh   zero, 14(t0)
        sh   zero, 16(t0)

        ld   fp, 0(sp)
        ld   ra, 8(sp)
        addi sp, sp, 16
        ret
    .end
