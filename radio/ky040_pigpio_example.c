// A  simple example of how to use the KY-040 with pigpio.h in C:
// Explanation
// Initialization: Start with gpioInitialise() to set up the GPIO.
// ISR Setup: Use gpioSetISRFunc() to attach an interrupt service routine (ISR) to pin A.
// Count Update: The update function checks the signals from pins A and B to update the
// count based on the direction of rotation.
// Continuous Loop: The main loop prints the current count every 500 milliseconds.
// sudo apt-get install pigpio
// gcc your_program.c -o your_program -lpigpio -lrt -lpthread



#include <stdio.h>
#include <pigpio.h>

#define PIN_A 17  // GPIO pin for A
#define PIN_B 18  // GPIO pin for B

volatile int count = 0;

void update(int gpio, int level, uint32_t tick) {
    int a = gpioRead(PIN_A);
    int b = gpioRead(PIN_B);
    count += (a == b) ? 1 : -1;  // Increment or decrement based on direction
}

int main() {
    if (gpioInitialise() < 0) {
        printf("GPIO initialization failed!\n");
        return -1;
    }

    gpioSetMode(PIN_A, PI_INPUT);
    gpioSetMode(PIN_B, PI_INPUT);

    gpioSetISRFunc(PIN_A, EITHER_EDGE, 0, update);  // Attach ISR for pin A

    while (1) {
        printf("Count: %d\n", count);  // Print the current count
        gpioDelay(500000);  // Delay for 500ms
    }

    gpioTerminate();  // Clean up
    return 0;
}
