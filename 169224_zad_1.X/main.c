/* 
 * File:   main.c
 * Author: Jakub Budzich - 169224
 *
 * Created on 24 marca 2025, 08:46
 */
#pragma config POSCMOD = NONE      // Primary Oscillator Select (HS Oscillator mode selected)
#pragma config OSCIOFNC = ON       // Primary Oscillator Output Function (OSC2/CLKO/RC15 functions as CLKO (FOSC/2))
#pragma config FCKSM = CSDCMD      // Clock Switching and Monitor (Clock switching and Fail-Safe Clock Monitor are disabled)
#pragma config FNOSC = FRC         // Oscillator Select (Primary Oscillator with PLL module (HSPLL, ECPLL))
#pragma config IESO = OFF          // Internal External Switch Over Mode (IESO mode (Two-Speed Start-up) disabled)

#pragma config WDTPS = PS32768     // Watchdog Timer Postscaler (1:32,768)
#pragma config FWPSA = PR128       // WDT Prescaler (Prescaler ratio of 1:128)
#pragma config WINDIS = ON         // Watchdog Timer Window (Standard Watchdog Timer enabled,(Windowed-mode is disabled))
#pragma config FWDTEN = OFF        // Watchdog Timer Enable (Watchdog Timer is disabled)
#pragma config ICS = PGx2          // Comm Channel Select (Emulator/debugger uses EMUC2/EMUD2)
#pragma config GWRP = OFF          // General Code Segment Write Protect (Writes to program memory are allowed)
#pragma config GCP = OFF           // General Code Segment Code Protect (Code protection is disabled)
#pragma config JTAGEN = OFF        // JTAG Port Enable (JTAG port is disabled)


#define _XTAL_FREQ 8000000
#include <xc.h>
#include <libpic30.h>
#include <stdlib.h>
#include "p24FJ128GA010.h"

unsigned portValue = 0; 
int numer_programu = 1; // zmienna do okreslania jaki program jest uruchomiony i do zmiany

// 8 biotwy licznik binarny zliczajacy w góre (0...255)
void binUP(){
    unsigned char licznik = 0;
    AD1PCFG = 0xFFFF;
    TRISA = 0x0000;
    while(1){
        LATA = licznik;
        __delay32(1000000);
        licznik++;
        if(licznik == 0)
            licznik == 0;
        // zmiana programu przyciskiem RD7 do przodu, RD6 do ty?u
        if(PORTDbits.RD6 == 0){
            numer_programu -= 1;
            break;
        }
        if(PORTDbits.RD7 == 0){
            numer_programu += 1;
            break;
        }
    }
}
// 8 bitowy licznik zliczajacy w dól (255...0)
void binDOWN(){
    unsigned char licznik = 0;
    AD1PCFG = 0xFFFF;
    TRISA = 0x0000;
    while(1){
        LATA = licznik;
        __delay32(1000000);
        licznik--;
        if(licznik == 0)
            licznik == 0;
        // zmiana programu przyciskiem RD7 do przodu, RD6 do ty?u
        if(PORTDbits.RD6 == 0){
            numer_programu -= 1;
            break;
        }
        if(PORTDbits.RD7 == 0){
            numer_programu += 1;
            break;
        }
    }
}
// 8 bitowy licznik w kodzie Graya zliczajacy w góre (repr. 0...255)
void grayUP(){
    unsigned char licznik = 0;
    unsigned char grayCode;
    AD1PCFG = 0xFFFF;
    TRISA = 0x0000;
    while(1) {
        grayCode = (licznik >> 1) ^ licznik;
        licznik++;
        LATA = grayCode;
        __delay32(1000000);
        // zmiana programu przyciskiem RD7 do przodu, RD6 do ty?u
        if(PORTDbits.RD6 == 0){
            numer_programu -= 1;
            break;
        }
        if(PORTDbits.RD7 == 0){
            numer_programu += 1;
            break;
        }
    }
}
// 8 bitowy licznik w kodzie Graya zliczaj?cy w dó? (repr. 255...0)

// 2x4 bitowy licznik w kodzie BCD zliczaj?cy w gór? (0...99)

// 2x4 bitowy licznik w kodzie BCD zliczaj?cy w dó? (99...0)

// 3 bitowy w??yk poruszaj?cy si? lewo-prawo

// Kolejka

// 6 bitowy generator liczb pseudolosowych oparty o konfiguracj? 11100111

int main(void) {
    if(numer_programu == 1){
        binUP();
    }
    if(numer_programu == 2){
        binDOWN();
    }
    if(numer_programu == 3){
        grayUP();
    }
    if(numer_programu < 1 || numer_programu > 3){
        numer_programu = 1;
    }
}

