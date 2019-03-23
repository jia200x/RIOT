/**
\brief This is a program which shows how to use the bsp modules to read the
   EUI64
       and leds.

\note: Since the bsp modules for different platforms have the same declaration,
       you can use this project with any platform.

Load this program on your boards. The LEDs should start blinking furiously.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, August 2014.
*/

#include "stdint.h"
#include "stdio.h"
// bsp modules required
#include "board_ow.h"
#include "eui64.h"

//=========================== main ============================================

/**
\brief The program starts executing here.
*/
int main(void) {

   uint8_t eui[8];

   // initialize the board
   board_init_ow();

   printf("Get euid now\n");
   eui64_get(eui);

   for (int i=0; i<8; i++){
    printf(" 0x%x", eui[i]);
   }
   printf("\nEnd main\n");
 }