// sudo apt-get install libx11-dev libxt-dev


//gcc poti.c -l wiringPi -lX11 -lXtst -o poti

/*
 +-----+-----+---------+------+---+---Pi 4B--+---+------+---------+-----+-----+
 | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 |     |     |    3.3v |      |   |  1 || 2  |   |      | 5v      |     |     |
 |   2 |   8 |   SDA.1 | ALT0 | 1 |  3 || 4  |   |      | 5v      |     |     |
 |   3 |   9 |   SCL.1 | ALT0 | 1 |  5 || 6  |   |      | 0v      |     |     |
 |   4 |   7 | GPIO. 7 |   IN | 0 |  7 || 8  | 1 | IN   | TxD     | 15  | 14  |
 |     |     |      0v |      |   |  9 || 10 | 1 | IN   | RxD     | 16  | 15  |
 |  17 |   0 | GPIO. 0 |   IN | 1 | 11 || 12 | 1 | ALT0 | GPIO. 1 | 1   | 18  |
 |  27 |   2 | GPIO. 2 |   IN | 1 | 13 || 14 |   |      | 0v      |     |     |
 |  22 |   3 | GPIO. 3 |   IN | 1 | 15 || 16 | 0 | IN   | GPIO. 4 | 4   | 23  |
 |     |     |    3.3v |      |   | 17 || 18 | 1 | IN   | GPIO. 5 | 5   | 24  |
 |  10 |  12 |    MOSI |   IN | 0 | 19 || 20 |   |      | 0v      |     |     |
 |   9 |  13 |    MISO |   IN | 0 | 21 || 22 | 0 | IN   | GPIO. 6 | 6   | 25  |
 |  11 |  14 |    SCLK |   IN | 0 | 23 || 24 | 1 | IN   | CE0     | 10  | 8   |
 |     |     |      0v |      |   | 25 || 26 | 1 | IN   | CE1     | 11  | 7   |
 |   0 |  30 |   SDA.0 |   IN | 1 | 27 || 28 | 1 | IN   | SCL.0   | 31  | 1   |
 |   5 |  21 | GPIO.21 |   IN | 1 | 29 || 30 |   |      | 0v      |     |     |
 |   6 |  22 | GPIO.22 |   IN | 1 | 31 || 32 | 1 | IN   | GPIO.26 | 26  | 12  |
 |  13 |  23 | GPIO.23 |   IN | 1 | 33 || 34 |   |      | 0v      |     |     |
 |  19 |  24 | GPIO.24 |   IN | 0 | 35 || 36 | 1 | IN   | GPIO.27 | 27  | 16  |
 |  26 |  25 | GPIO.25 |   IN | 0 | 37 || 38 | 0 | IN   | GPIO.28 | 28  | 20  |
 |     |     |      0v |      |   | 39 || 40 | 0 | IN   | GPIO.29 | 29  | 21  |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |
 +-----+-----+---------+------+---+---Pi 4B--+---+------+---------+-----+-----+
*/
#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>

#define DT_R 22//GPIO22 (P31) grey
#define CLK_R 23 //GPIO23 (P33) amber
#define SW_R 21 //GPIO21 (P29) white

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

Display *disp;


/* Send Fake Key Event */
static void SendKey (Display * disp, KeySym keysym){
 KeyCode keycode = 0;
 keycode = XKeysymToKeycode (disp, keysym);
 if (keycode == 0) return;
 XTestGrabControl (disp, True);
 /* Generate regular key press and release */
 XTestFakeKeyEvent (disp, keycode, True, 0);
 XTestFakeKeyEvent (disp, keycode, False, 0);
 XSync (disp, False);
 XTestGrabControl (disp, False);
}

void interrupt_LRot(void) {
//        fprintf(stderr, "x");
        if(digitalRead(DT_R) && digitalRead(CLK_R))  {
            state_3 = STATE_NONE;
        }

        if(!digitalRead(DT_R) && digitalRead(CLK_R)) {
            if(state_3 == STATE_NONE) {
                state_3 = STATE_LEFT_1;
            }
            else if(state_3 == STATE_RIGHT_2) {
                state_3 = STATE_RIGHT_3;
                SendKey (disp, XK_Right);
//                fprintf(stdout, "Right\n");
            }
        }

        if(digitalRead(DT_R) && !digitalRead(CLK_R)) {
            if(state_3 == STATE_NONE) {
                state_3 = STATE_RIGHT_1;
            }
            else if(state_3 == STATE_LEFT_2) {
                state_3 = STATE_LEFT_3;
                SendKey (disp, XK_Left);
//                fprintf(stdout, "Left\n");
            }
        }

        if(!digitalRead(DT_R) && !digitalRead(CLK_R)) {
            if(state_3 == STATE_LEFT_1) {
                state_3 = STATE_LEFT_2;
            }
            else if(state_3 == STATE_RIGHT_1) {
                state_3 = STATE_RIGHT_2;
            }
        }
}


void interrupt_RPush(void){
   if(!digitalRead(SW_R))
      usleep(5000); //debouncing
   if(!digitalRead(SW_R)){
      system ("/home/radio/radio/rpush");
      fprintf(stderr, "Right button pushed\n");
   }
}

int main() {

//    XInitThreads();  // to fix error xcb unknown request in queue while dequeuing 
    disp = XOpenDisplay (NULL);
    fprintf(stderr, "started...");
    if(wiringPiSetup() == -1) {
        fprintf(stderr, "wiring error");
        return -1;
    }

    pinMode(DT_R,  INPUT);
    pinMode(CLK_R, INPUT);
    pinMode(SW_R,  INPUT);
    pullUpDnControl(SW_R, PUD_UP);
    wiringPiISR (DT_R, INT_EDGE_BOTH, interrupt_LRot) ;
    wiringPiISR (CLK_R, INT_EDGE_BOTH, interrupt_LRot) ;
    wiringPiISR (SW_R, INT_EDGE_FALLING, interrupt_RPush) ;


    while(1) {
      fprintf(stderr, ".");
      // waitForInterrupt
       sleep(10);
    }
}
