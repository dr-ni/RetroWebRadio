
// gcc your_program.c -o your_program -lpigpio -lrt -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <pigpio.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>

#define DT_L 2  // GPIO02 (P15)
#define CLK_L 0  // GPIO00 (P13)
#define SW_L 25  // GPIO25 (P37)

#define STATE_NONE 0
#define STATE_LEFT_1 1
#define STATE_LEFT_2 2
#define STATE_LEFT_3 3
#define STATE_RIGHT_1 4
#define STATE_RIGHT_2 5
#define STATE_RIGHT_3 6

int value = 0;
int state = STATE_NONE;
int state_2 = STATE_NONE;
int state_3 = STATE_NONE;

void interrupt_LRot(int gpio, int level, uint32_t tick) {
    if (gpio == DT_L) {
        if (gpioRead(DT_L) && gpioRead(CLK_L)) {
            state_3 = STATE_NONE;
        }

        if (!gpioRead(DT_L) && gpioRead(CLK_L)) {
            if (state_3 == STATE_NONE) {
                state_3 = STATE_LEFT_1;
            } else if (state_3 == STATE_RIGHT_2) {
                state_3 = STATE_RIGHT_3;
                system("/usr/bin/mpc volume -2 &");
            }
        }

        if (gpioRead(DT_L) && !gpioRead(CLK_L)) {
            if (state_3 == STATE_NONE) {
                state_3 = STATE_RIGHT_1;
            } else if (state_3 == STATE_LEFT_2) {
                state_3 = STATE_LEFT_3;
                system("/usr/bin/mpc volume +2 &");
            }
        }

        if (!gpioRead(DT_L) && !gpioRead(CLK_L)) {
            if (state_3 == STATE_LEFT_1) {
                state_3 = STATE_LEFT_2;
            } else if (state_3 == STATE_RIGHT_1) {
                state_3 = STATE_RIGHT_2;
            }
        }
    } else if (gpio == CLK_L) {
        // Clock pin logic (if needed)
    }
}

void interrupt_LPush(int gpio, int level, uint32_t tick) {
    if (!gpioRead(SW_L)) {
        usleep(5000); // Debouncing
    }
    if (!gpioRead(SW_L)) {
        system("/home/radio/radio/lpush");
        fprintf(stderr, "Left button pushed\n");
    }
}

int main() {
    fprintf(stderr, "started...\n");
    if (gpioInitialise() < 0) {
        fprintf(stderr, "GPIO initialization error\n");
        return -1;
    }

    gpioSetMode(DT_L, PI_INPUT);
    gpioSetMode(CLK_L, PI_INPUT);
    gpioSetMode(SW_L, PI_INPUT);
    gpioSetPullUpDown(SW_L, PI_PUD_UP);

    gpioSetISRFunc(DT_L, EITHER_EDGE, 0, interrupt_LRot);
    gpioSetISRFunc(CLK_L, EITHER_EDGE, 0, interrupt_LRot);
    gpioSetISRFunc(SW_L, FALLING_EDGE, 0, interrupt_LPush);

    while (1) {
        fprintf(stderr, ".");
        sleep(10);  // Main loop delay
    }

    gpioTerminate();  // Clean up before exiting
    return 0;
}
