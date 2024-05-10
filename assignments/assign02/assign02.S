#include "hardware/regs/addressmap.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/regs/timer.h"
#include "hardware/regs/m0plus.h"

.syntax unified                                                 @ Specify unified assembly syntax
.cpu    cortex-m0plus                                           @ Specify CPU type is Cortex M0+
.thumb                                                          @ Specify thumb assembly for RP2040
.global main_asm                                                @ Provide program starting address to the linker
.align 4                                                        @ Specify code alignment

.equ    ALARM_TIME, 1000000                                     @ Specify the default alarm timeout

.equ    GPIO_BTN_RISE_MSK, 0x00400000                           @ Bit-22 for falling-edge event on GP21
.equ    GPIO_BTN_FALL_MSK, 0x00800000                           @ Bit-23 for rising-edge event on GP21

@ Input time for a dot is between 0 and 255 ms
@ Input time for a dash is 255 ms and greater
.equ    DOT, 255                                                

.equ    TIMESTAMP_DEFAULT, 0                                    @ Set timestamps to 0 by default

.equ    GPIO_BTN,  21                                           @ Specify pin for the button
.equ    GPIO_LED_PIN, 28                                        @ Specify pin for the built-in LED
.equ    GPIO_DIR_IN,   0                                        @ Specify input direction for a GPIO pin
.equ    GPIO_DIR_OUT,  1                                        @ Specify output direction for a GPIO pin

.equ    GPIO_ISR_OFFSET, 0x74                                   @ GPIO is int #13 (vector table entry 29)
.equ    ALARM_ISR_OFFSET, 0x40                                  @ ALARM0 is int #0 (vector table entry 16)

@ Entry point to the ASM portion of the program
main_asm:
    bl      init_gp21                       @ Initialise and configure GP21 for use
    bl      install_gp21_interrupt          @ Install GP21 interrupt
    bl      install_alarm_interrupt         @ Install alarm interrupt
    bl      set_alarm                       @ Set first alarm

    movs     R6, #0                         @ global register to check for first input


@ wait for initial input to let user read prompt before saving inputs.
wait_for_input:
    cmp     R6, #0                  @ any initial button interrupt changes this register
    bgt     continue_main 
    b       wait_for_input


continue_main:                                   @ continue accepting inputs
    bl      level_on                             @ check further inputs
    b       continue_main                        @ Infinite loop

@ Initialises GP21
@ Also sets their direction, and enables a falling-edge interrupt.
init_gp21:
    push    {lr}                            @ Push link register to stack

    movs    r0, #GPIO_BTN                   @ Set 'pin' parameter to GP21 for asm_gpio_init
    bl      asm_gpio_init                   @ Initialise GP21 button
    movs    r0, #GPIO_BTN                   @ Set 'pin' parameter to GP21 for asm_gpio_set_dir
    movs    r1, #GPIO_DIR_IN                @ Set 'out' parameter to IN for asm_gpio_set_dir
    bl      asm_gpio_set_dir                @ Set direction for GP21 button to IN
    movs    r0, #GPIO_BTN                   @ Set 'pin' parameter to GP21 for asm_gpio_set_irq_fall
    bl      asm_gpio_set_irq_fall           @ Enable falling-edge interrupt for GP21
    movs    r0, #GPIO_BTN                   @ Set 'pin' paramater to GP21 for asm_gpio_set_irq_rise
    bl      asm_gpio_set_irq_rise           @ Enable rising-edge interrupt for GP21

    pop     {pc}                            @ Continue with program

@ Installs the GP21 interrupt handler
install_gp21_interrupt:
    push    {lr}                            @ Push link register to stack

    ldr     r3, =PPB_BASE                   @ Load base address for VTOR and NVIC into r3
    
    ldr     r2, =M0PLUS_VTOR_OFFSET         @ Load offset for VTOR into r2
    adds    r1, r2, r3                      @ Add base address and VTOR offset into r1
    ldr     r1, [r1]                        @ Load address of RAM vector table into r1
    movs    r2, #GPIO_ISR_OFFSET            @ Move offset for GP21 ISR into r2
    adds    r1, r2                          @ Add offset to VTOR address to get address for storing ISR handler in r1
    ldr     r0, =handle_gp21_interrupt      @ Load address of ISR handler into r0
    str     r0, [r1]                        @ Store address of ISR handler in address in r1

    ldr     r0, =0x00002000                 @ Load value with 13th bit set into r0
    ldr     r2, =M0PLUS_NVIC_ICPR_OFFSET    @ Load offset for ICPR into r2
    adds    r1, r2, r3                      @ Add base address and ICPR offset into r1
    ldr     r2, [r1]                        @ Load value of of ICPR into r2
    orrs    r2, r0                          @ Set 13th bit of ICPR
    str     r2, [r1]                        @ Store new value of ICPR into address in r1 
    ldr     r2, =M0PLUS_NVIC_ISER_OFFSET    @ Load offset for ISER into r2
    adds    r1, r2, r3                      @ Add base address and ISER offset into r1
    ldr     r2, [r1]                        @ Load value of ISE R into r2
    orrs    r2, r0                          @ Set 13th bit of ISER
    str     r2, [r1]                        @ Store new value of ISER into address in r1

    pop     {pc}                            @ Continue with program

@ Installs the alarm interrupt handler
install_alarm_interrupt:
    push    {lr}                            @ Push link register to stack

    ldr     r2, =PPB_BASE                   @ Load base address for VTOR and NVIC into r2
    ldr     r1, =M0PLUS_VTOR_OFFSET         @ Load VTOR offset into r1
    adds    r2, r1                          @ Add r1 and r2 into r2 to get address of VTOR
    ldr     r1, [r2]                        @ Load address of RAM vector table into r1
    ldr     r2, =ALARM_ISR_OFFSET           @ Load alarm ISR offset into r2
    adds    r2, r1                          @ Add r1 and r2 to get address of alarm ISR
    ldr     r0, =handle_alarm_interrupt     @ Load address of alarm interrupt handler into r0
    str     r0, [r2]                        @ Store that at address of alarm ISR

    ldr     r0, =0x1                        @ Set r0 to 1
    ldr     r2, =PPB_BASE                   @ Load base address for VTOR and NVIC into r2
    ldr     r1, =M0PLUS_NVIC_ICPR_OFFSET    @ Load offset for NVIC ICPR into r1
    adds    r1, r2                          @ Add r1 and r2 to get address of ICPR
    str     r0, [r1]                        @ Store 1 in ICPR
    ldr     r1, =M0PLUS_NVIC_ISER_OFFSET    @ Load offset for NVIC ISER into r1
    adds    r1, r2                          @ Add r1 and r2 to get address of ISER
    str     r0, [r1]                        @ Store 1 in ISER

    pop     {pc}                            @ Continue with program

@ Sets an alarm for time in ALARM_TIME
set_alarm:
    push    {lr}                            @ Push link register to stack

    ldr     r2, =TIMER_BASE                 @ Load base address for timer component into r2
    ldr     r1, =0x1                        @ Set r1 to 1 for next instruction
    str     r1, [r2, #TIMER_INTE_OFFSET]    @ Store 1 in the interrupt enable register
    ldr     r1, [r2, #TIMER_TIMELR_OFFSET]  @ Load lower 32 bits of timer register into r1
    ldr     r3, =ALARM_TIME                 @ Load timer length into r3
    adds    r1, r3                          @ Add offset address and timer length into r1
    str     r1, [r2, #TIMER_ALARM0_OFFSET]  @ Store result in alarm0 control register

    pop     {pc}                            @ Continue with program

@ Level is happening
level_on:
    cmp     r7, #1                          @ Compare r7 to 1
    beq     handle_space                    @ If 1, append space to input

    cmp     r7, #2                          @ Compare r7 to 2
    bgt     handle_end                      @ If 2, end current input

continue_level:
    b       level_on                        @ Continue looping

.thumb_func
@ Handle the GP21 interrupt
@ Will determine whether the interrupt is falling-edge or rising-edge
handle_gp21_interrupt:
    push    {lr}                            @ Push link register to stack

    ldr     r2, =IO_BANK0_BASE              @ Load base address for the IO component into r2
    ldr     r3, =IO_BANK0_PROC0_INTS2_OFFSET       @ Load interrupt status register offset into r3
    adds    r2, r3                          @ Add to get address of interrupt status register
    ldr     r0, [r2]                        @ Load contents of interrupt status register into r2
    bl      clear_interrupt                 @ Clear interrupt

    ldr     r3, =GPIO_BTN_RISE_MSK          @ Load mask for rising edge events into r3
    cmp     r0, r3                          @ Test if the interrupt is rising edge
    beq     handle_rising_edge              @ If it is, handle rising edge    
    b       handle_falling_edge             @ Else, handle falling edge

@ Handles a falling edge interrupt
handle_rising_edge:
    bl      get_time_in_ms                  @ Get timestamp in ms
    ldr     r1, =lpress_timestamp           @ Load address of lpress_timestamp into r1
    str     r0, [r1]                        @ Store timestamp in lpress_timestamp

    pop     {pc}                            @ Continue with program

@ Handles a rising edge interrupt
handle_falling_edge:
    bl      get_time_in_ms                  @ Get timestamp in ms
    ldr     r1, =lrelease_timestamp         @ Load address of lrelease_timestamp into r1
    str     r0, [r1]                        @ Store timestamp in lrelease_timestamp

    bl      set_alarm                       @ Reset alarm

    ldr     r0, =lrelease_timestamp         @ Load address of lrelease_timestamp into r0
    ldr     r0, [r0]                        @ Load release timestamp into r0
    ldr     r1, =lpress_timestamp           @ Load address of lpress_timestamp into r1
    ldr     r1, [r1]                        @ Load press timestamp into r1
    bl      get_time_difference             @ Get difference between the two timestamps
    movs    r7, r0                          @ Move difference to r7 for watchdog_update
    bl      watchdog_update                 @ Call watchdog update
    movs    r0, r7                          @ Load difference into r0
    ldr     r7, =0x0                        @ Load 0 into r7

    cmp     r0, DOT                         @ Compare difference and 255
    blt     handle_dot                      @ If difference < 255 branch to handle_dot
    b       handle_dash                     @ Else handle dash

@ Clears a GP21 interrupt
@ Takes one parameter, the mask for the relevant interrupt
clear_interrupt:
    push    {lr}                            @ Push link register to stack

    ldr     r1, =IO_BANK0_BASE              @ Load base address for the IO component into r1
    ldr     r2, =IO_BANK0_INTR2_OFFSET      @ Load offset address for GPIO raw interrupts register into r2
    add     r1, r2                          @ Add into r1 to get address of GPIO raw interrupts register
    str     r0, [r1]                        @ Store mask in GPIO raw interrupts register

    pop     {pc}                            @ Continue with program

@ Handles a dot input
handle_dot:
    ldr     r0, =0x1                        @ Load 1 as 'input' parameter for detectInput
    movs    R6, R0                          @ keep in global register for mainline program
    bl      detectInput                     @ Call detectInput to add dot to input sequence

    pop     {pc}                            @ Continue with program counter

@ Handles a dash input
handle_dash:
    ldr     r0, =0x2                        @ Load 2 as 'input' parameter for detectInput
    movs    R6, R0                          @ keep in global register for mainline program
    bl      detectInput                     @ Call detectInput to add dash to input sequence       

    pop     {pc}                            @ Continue with program counter

@ Handles a space input
handle_space:
    ldr     r0, =0x3                        @ Load 3 as 'input' parameter for detectInput
    movs    R6, R0                          @ keep in global register for mainline program
    bl      detectInput                     @ Call detectInput to add dash to input sequence

    ldr     r1, =0x1                        @ Load 1 into r1 for next instruction
    adds    r7, r1                          @ Add 1 to r7

    b       continue_main                  @ Continue with program

@ Handles an end input (two seconds delay)
handle_end:
    ldr     r0, =msg_end                    @ Load message for debugging purposes
    bl      printf                          @ Print message

    ldr     r0, =0x4                        @ Load 4 as 'input' parameter for detectInput
    bl      detectInput                     @ Call detectInput to add dash to input sequence

    ldr     r7, =0x0                        @ Load 0 into r7

    movs    R6, #0                          @ reset to wait for input again after enter
    b       wait_for_input                  @ wait for input

.thumb_func
@ Handles the alarm ISR
@ Will end the game
handle_alarm_interrupt:
    push    {lr}                            @ Push link register to stack
    
    ldr     r2, =TIMER_BASE                 @ Load base address for timer component into r2
    ldr     r1, =0x1                        @ Set r1 to 1 for next instruction
    str     r1, [r2, #TIMER_INTR_OFFSET]    @ Store 1 in the INTR

    ldr     r1, =0x1                        @ Load 1 into r1 for next instruction
    adds    r7, r1                          @ Add 1 to r7

    bl      set_alarm                       @ Set alarm

    pop     {pc}                            @ Continue with program

.align 4
msg_test:  .asciz "Code has reached here!\n"
msg_both:  .asciz "Quick press detected!\n"
msg_fall:  .asciz "Falling-edge interrupt triggered!\n"
msg_rise:  .asciz "Rising-edge interrupt triggered!\n"
msg_dot:   .asciz "Dot detected!\n"
msg_dash:  .asciz "Dash detected!\n"
msg_space: .asciz "Space detected!\n"
msg_end:   .asciz "End of input detected!\n"
msg_alarm: .asciz "Alarm triggered!\n"

.data
lpress_timestamp:   .word TIMESTAMP_DEFAULT
lrelease_timestamp: .word TIMESTAMP_DEFAULT

@ defining the character set for the Morse code
print_dot:    .asciz ".\n"
print_dash:   .asciz "-\n"
print_space:  .asciz " \n"
print_enter:  .asciz "n\n"