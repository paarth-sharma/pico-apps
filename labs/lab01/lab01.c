#include "pico/stdlib.h"

/**
 * @brief Toggle LED state and sleep for a specified delay.
 * 
 * @param pin   The pin number of the LED.
 * @param delay The delay time in milliseconds.
 */

void toggleLED(uint pin, uint delay){
  gpio_put(pin, 1); // Toggle the LED on
  sleep_ms(delay);  // Sleep for the specified delay
  gpio_put(pin, 0); // Toggle the LED off
  sleep_ms(delay);  // Sleep for the specified delay
}

int main(){
  // Specify the PIN number and sleep delay(milliseconds)
  const uint LED_PIN   =  25;
  const uint LED_DELAY = 500;

  // Setup the LED pin as an output.
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  // Do forever...
  while (true){
    toggleLED(LED_PIN, LED_DELAY); // Call the subroutine to toggle LED state
  }

  // Should never get here due to infinite while-loop.
  return 0;
}
