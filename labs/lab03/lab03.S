.syntax unified                 @ Specify unified assembly syntax
.cpu    cortex-m0plus           @ Specify CPU type is Cortex M0+
.thumb                          @ Specify thumb assembly for RP2040
.global main_asm                @ Provide program starting address to the linker
.align 4                        @ Specify code alignment

.equ    SLEEP_TIME, 500         @ Specify the amount of ms that we want to sleep for in the loop
.equ    LED_GPIO_PIN, 25        @ Specifiy the physical GPIO pin that the LED is connected to
.equ    LED_GPIO_OUT, 1         @ Specify the direction that we want to set the GPIO pin to
.equ    LED_VALUE_ON, 1         @ Specify the value that turns the LED "ON"
.equ    LED_VALUE_OFF, 0        @ Specify the value that turns the LED "OFF"
.equ    BUTTON_PIN, 21          @ initialising pin number used for push_button
.equ    button_state_1, 1       @ for button state comaparison
.equ    button_state_0, 0       @ for button state comaparison
.equ    PAUSE_TIME, 5           @short buffer for less lag bw state changes

@ main_asm definition
main_asm:
    movs    r0, #LED_GPIO_PIN   @ This value is the GPIO LED pin on the PI PICO board
    bl      asm_gpio_init       @ Call the subroutine to initialise the GPIO pin specified by r0        
    movs    r0, #LED_GPIO_PIN   @ This value is the GPIO LED pin on the PI PICO board        
    movs    r1, #LED_GPIO_OUT   @ We want this GPIO pin to be setup as an output pin        
    bl      asm_gpio_set_dir    @ Call the subroutine to set the GPIO pin specified by r0 to state specified by r1       
    movs    r0, #BUTTON_PIN     @ set up button to be an input pin        
    bl      asm_gpio_init       @ initialising button pin      
    movs    r0, #BUTTON_PIN
    movs    r1, button_state_0  @ for button state comaparison     
    bl      asm_gpio_set_dir

timeout:
    ldr     r0, =PAUSE_TIME             @ Load PAUSE_TIME into r0
    bl      sleep_ms                    @ Call sleep_ms function
    movs    r0, #BUTTON_PIN             @ Load BUTTON_PIN into r0
    bl      asm_gpio_get                @ Call asm_gpio_get function
    cmp     r0, button_state_1          @ Compare the button state with button_state_1
    beq     timeout                     @ Branch to timeout if equal
       
loop:
    ldr     r0, =SLEEP_TIME             @ Load SLEEP_TIME into r0
    bl      sleep_ms                    @ Call sleep_ms function
    bl      sub_toggle                  @ Call sub_toggle subroutine
    movs    r0, #BUTTON_PIN             @ Load BUTTON_PIN into r0
    bl      asm_gpio_get                @ Call asm_gpio_get function
    cmp     r0, button_state_1          @ Compare the button state with button_state_1
    beq     timeout                     @ Branch to timeout if equal
    b       loop                        @ Branch to loop

sub_toggle:
    push    {lr}                        @ to push lr onto the stack
    movs    r0, #LED_GPIO_PIN           @ Load LED_GPIO_PIN into r0
    bl      asm_gpio_get                @ Call asm_gpio_get function
    cmp     r0, #LED_VALUE_OFF          @ Compare the LED state with LED_VALUE_OFF
    beq     led_set_on                  @ Branch to led_set_on if equal

led_set_off:
    movs    r1, #LED_VALUE_OFF          @ Load LED_VALUE_OFF into r1
    b       led_set_state               @ Branch to led_set_state

led_set_on:
    movs    r1, #LED_VALUE_ON           @ Load LED_VALUE_ON into r1

led_set_state:
    movs    r0, #LED_GPIO_PIN           @ Load LED_GPIO_PIN into r0
    bl      asm_gpio_put                @ Call asm_gpio_put function
    pop     {pc}                        @ to pop pc from the stack

.data
    .align 4
