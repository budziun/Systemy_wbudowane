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
volatile uint8_t flaga = 0; // flaga informujaca o zmianie programu
// funkcja opoznienie
void delay(uint32_t ilosc) {
    uint32_t i, j;

    for(i = 0; i < ilosc; i++) {
        for(j = 0; j < 100; j++) {
            asm("NOP");
        }
    }
}
// Inicjalizacja port�w i przerwan
void init() {
    AD1PCFG = 0xFFFF;
    TRISA = 0x0000;  // Port A jako wyjscie
    TRISD = 0xFFFF;  // Port D jako wejscie
    
    // Konfiguracja przerwan od pin�w
    CNPU2bits.CN19PUE = 1;  // Pull-up dla RD13
    CNPU1bits.CN15PUE = 1;  // Pull-up dla RD6 
    
    // Wlaczenie przerwan Change Notification dla przycisk�w
    CNEN1bits.CN15IE = 1;   // Wlacz przerwanie dla RD6
    CNEN2bits.CN19IE = 1;   // Wlacz przerwanie dla RD13
    
    IFS1bits.CNIF = 0;      // Wyczysc flage przerwania CN
    IEC1bits.CNIE = 1;      // Wlacz przerwania CN
}

// Procedura obslugi przerwania przyciskami 
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void) {
    __delay32(200);
    // Sprawdzenie, kt�ry przycisk zostal nacisniety
    // poprzedni program
    if(PORTDbits.RD13 == 0) {
        numer_programu--;
        flaga = 1;
    }
    // nastepny program
    else if(PORTDbits.RD6 == 0) {
        numer_programu++;
        flaga = 1;
    }
    
    while(PORTDbits.RD13 == 0 || PORTDbits.RD6 == 0);
    // Wyczysc flage
    IFS1bits.CNIF = 0;
}
//1. 8 bitowy licznik binarny zliczajacy w g�re (0...255)
void binUP(){
    unsigned char licznik = 0;
    flaga = 0;
    while(!flaga) {
        LATA = licznik++;
        delay(750);
    }
}
//2. 8 bitowy licznik zliczajacy w d�l (255...0)
void binDOWN(){
    unsigned char licznik = 255;
    flaga = 0;
    while(!flaga) {
        LATA = licznik--;
        delay(750);
    }
}
//3. 8 bitowy licznik w kodzie Graya zliczajacy w g�re (repr. 0...255)
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
//4. 8 bitowy licznik w kodzie Graya zliczajacy w d�l (repr. 255...0)
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
//5. 2x4 bitowy licznik w kodzie BCD zliczajacy w g�re (0...99)
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
//6. 2x4 bitowy licznik w kodzie BCD zliczajacy w d�l (99...0)
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
//7. 3 bitowy wezyk poruszajacy sie lewo-prawo
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
        // Reset kolejki po zapelnieniu
        kolejka = 0b00000000;
        
        for (uint16_t i = 0; i < 8 && !flaga; i++) {
            wzor = 0b00000001;
            for (uint16_t j = 0; j < 8 - i && !flaga; j++) {
                LATA = kolejka | wzor;
                delay(600);
                // Sprawdzenie czy wyjsc z petli
                if (flaga) {
                    return;
                }
                // Przesuniecie wzorku
                if (j != 7 - i)
                    wzor <<= 1;
            }
            // dodanie aktualnej diody do reszty kolejki
            kolejka |= wzor;
        }
        
        // Wszystkie diody sie swieca - wyswietl przez chwile
        LATA = 0b11111111;
        delay(1000); 
        if (flaga) {
            return;
        }

    }
}
//9. 6 bitowy generator liczb pseudolosowych oparty o konfiguracje 11100111
void losowe() {
    unsigned char lcg = 0b11100111;  // wartosc poczatkowa
    const unsigned char a = 17;  // mnoznik
    const unsigned char c = 43;  // stala dodawana
    flaga = 0;

    while (!flaga) {
        LATA = lcg & 0x3F;            // wyj?cie na port i uciecie dwoch najstarszych bitow
        delay(1000);
        //LCG z maskowaniem 6-bitowym
        lcg = (a * lcg + c) & 0x3F;  // 0x3F -> maska 0b00111111
        
    }
}
// Gl�wna funkcja programu z wyborem programu
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
