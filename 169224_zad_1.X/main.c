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

volatile uint16_t numer_programu = 1;
volatile uint8_t flaga = 0; // flaga informuj?ca o zmianie programu
// funkcja delay
void delay(uint32_t ilosc) {
    uint32_t i, j;

    for(i = 0; i < ilosc; i++) {
        for(j = 0; j < 100; j++) {
            asm("NOP");
        }
    }
}
// Inicjalizacja portów i przerwa?
void init() {
    AD1PCFG = 0xFFFF;
    TRISA = 0x0000;  // Port A jako wyj?cie
    TRISD = 0xFFFF;  // Port D jako wej?cie
    
    // Konfiguracja przerwa? od pinów
    CNPU2bits.CN19PUE = 1;  // Pull-up dla RD13
    CNPU1bits.CN15PUE = 1;  // Pull-up dla RD6 
    
    // W??czenie przerwa? Change Notification dla przycisków
    CNEN1bits.CN15IE = 1;   // W??cz przerwanie dla RD6
    CNEN2bits.CN19IE = 1;   // W??cz przerwanie dla RD13
    
    IFS1bits.CNIF = 0;      // Wyczy?? flag? przerwania CN
    IEC1bits.CNIE = 1;      // W??cz przerwania CN
}

// Procedura obs?ugi przerwania Change Notification
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void) {
    __delay32(200);
    // Sprawdzenie, który przycisk zosta? naci?ni?ty
    // poprzedni program
    if(PORTDbits.RD13 == 0) {
        numer_programu--;
        flaga = 1;
    }
    // nast?pny program
    else if(PORTDbits.RD6 == 0) {
        numer_programu++;
        flaga = 1;
    }
    
    while(PORTDbits.RD13 == 0 || PORTDbits.RD6 == 0);
    // Wyczy?? flag?
    IFS1bits.CNIF = 0;
}
//1. 8 bitowy licznik binarny zliczaj?cy w gór? (0...255)
void binUP(){
    unsigned char licznik = 0;
    flaga = 0;
    while(!flaga) {
        LATA = licznik++;
        delay(750);
    }
}
//2. 8 bitowy licznik zliczaj?cy w dó? (255...0)
void binDOWN(){
    unsigned char licznik = 255;
    flaga = 0;
    while(!flaga) {
        LATA = licznik--;
        delay(750);
    }
}
//3. 8 bitowy licznik w kodzie Graya zliczaj?cy w gór? (repr. 0...255)
void grayUP(){
    unsigned char licznik = 0;
    unsigned char grayCode;
    flaga = 0;
    
    while(!flaga) {
        grayCode = (licznik >> 1) ^ licznik;
        LATA = grayCode;
        licznik++;
        delay(750);
    }
}
//4. 8 bitowy licznik w kodzie Graya zliczaj?cy w dó? (repr. 255...0)
void grayDOWN(){
    unsigned char licznik = 255;
    unsigned char grayCode;
    flaga = 0;
    
    while(!flaga) {
        grayCode = (licznik >> 1) ^ licznik;
        LATA = grayCode;
        licznik--;
        delay(750);
    }
}
//5. 2x4 bitowy licznik w kodzie BCD zliczaj?cy w gór? (0...99)
void bcdUP(){
    unsigned char dziesiatki = 0;
    unsigned char jednosci = 0;
    flaga = 0;
    
    while(!flaga) {
        LATA = (dziesiatki << 4) | jednosci;
        delay(750);
        
        jednosci++;
        if (jednosci > 9) {
            jednosci = 0;
            dziesiatki++;
        }
        
        if (dziesiatki > 9) {
            dziesiatki = 0;
        }
    }
}
//6. 2x4 bitowy licznik w kodzie BCD zliczaj?cy w dó? (99...0)
void bcdDOWN() {
    unsigned char dziesiatki = 9;
    unsigned char jednosci = 9;
    flaga = 0;
    
    while(!flaga) {
        LATA = (dziesiatki << 4) | jednosci;
        delay(750);
        
        if (jednosci == 0) {
            jednosci = 9;
            dziesiatki--;
        } else {
            jednosci--;
        }
        
        if (dziesiatki == 0 && jednosci == 0) {
            dziesiatki = 9;
            jednosci = 9;
        }
    }
}
//7. 3 bitowy w??yk poruszaj?cy si? lewo-prawo
void snake() {
    unsigned char wez = 0b00000111;
    uint16_t kierunek = 1;
    flaga = 0;
    
    while(!flaga) {
        LATA = wez;
        delay(750);
        
        if (kierunek == 1) {
            wez <<= 1;
            if (wez > 0b01111000) {
                wez = 0b11100000;
                kierunek = -1;
            }
        }
        else if (kierunek == -1) {
            wez >>= 1;
            if (wez < 0b00000111) {
                wez = 0b00000111;
                kierunek = 1;
            }
        }
    }
}
//8. Kolejka
void kolejka() {
    unsigned char kolejka = 0b00000000;
    unsigned char wzor;
    flaga = 0;
    
    while(!flaga) {
        // Reset kolejki po zape?nieniu
        kolejka = 0b00000000;
        
        for (uint16_t i = 0; i < 8 && !flaga; i++) {
            wzor = 0b00000001;
            for (uint16_t j = 0; j < 8 - i && !flaga; j++) {
                LATA = kolejka | wzor;
                delay(600);
                // Sprawdzenie czy wyj?? z p?tli
                if (flaga) {
                    return;
                }
                // Przesuni?cie wzorku
                if (j != 7 - i)
                    wzor <<= 1;
            }
            // dodanie aktualnej diody do reszty kolejki
            kolejka |= wzor;
        }
        
        // Wszystkie diody si? ?wiec? - wy?wietl przez chwil?
        LATA = 0b11111111;
        delay(1000); 
        if (flaga) {
            return;
        }

    }
}
//9. 6 bitowy generator liczb pseudolosowych oparty o konfiguracj? 11100111
void losowe() {
    unsigned char lfsr = 0x21;
    unsigned char bit;
    flaga = 0;
    
    while(!flaga) {
        LATA = lfsr & 0x3F;
        delay(600);
        
        bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
        lfsr = (lfsr >> 1) | (bit << 5);
    }
}
// G?ówna funkcja programu z wyborem programu
int main(void) {
    init();
    
    while(1) {
        if(numer_programu < 1) {
            numer_programu = 9;
        } else if(numer_programu > 9) {
            numer_programu = 1;
        }
        switch(numer_programu) {
            case 1:
                binUP();
                break;
            case 2:
                binDOWN();
                break;
            case 3:
                grayUP();
                break;
            case 4:
                grayDOWN();
                break;
            case 5:
                bcdUP();
                break;
            case 6:
                bcdDOWN();
                break;
            case 7:
                snake();
                break;
            case 8:
                kolejka();
                break;
            case 9:
                losowe();
                break;
        }
    }
    return 0;
}
