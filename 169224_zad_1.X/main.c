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
uint16_t numer_programu = 1; // zmienna do okreslania jaki program jest uruchomiony i do zmiany
// Inicjalizacja portów
void init() {
    AD1PCFG = 0xFFFF;
    TRISA = 0x0000;
    TRISD = 0xFFFF; // Ustawienie portu D jako wej?cie
}

// Funkcja do sprawdzania przycisków 
int zmiana_przycisku() {
    if(PORTDbits.RD6 == 0) { 
        __delay32(100000);
        while(PORTDbits.RD6 == 0);  // Czekaj, a? przycisk zostanie puszczony
        return -1; // numer programu -1
    }
    if(PORTDbits.RD7 == 0) {
        __delay32(100000);
        while(PORTDbits.RD7 == 0);  // Czekaj, a? przycisk zostanie puszczony
        return 1; // numer programu +1
    }
    return 0;
}

// 8 bitowy licznik binarny zliczajacy w góre (0...255)
void binUP(){
    unsigned char licznik = 0;
    while(1) {
        LATA = licznik++;
        __delay32(1000000);  // Zmienna opó?niaj?ca dla p?ynnego liczenia
        uint16_t button = zmiana_przycisku();
        if(button) { numer_programu += button; break; }
    }
}

// 8 bitowy licznik zliczajacy w dó? (255...0)
void binDOWN(){
    unsigned char licznik = 255;
    while(1) {
        LATA = licznik--;
        __delay32(1000000);  // Zmienna opó?niaj?ca dla p?ynnego liczenia
        uint16_t button = zmiana_przycisku();
        if(button) { numer_programu += button; break; }
    }
}

// 8 bitowy licznik w kodzie Graya zliczajacy w góre (repr. 0...255)
void grayUP(){
    unsigned char licznik = 0;
    unsigned char grayCode;
    while(1) {
        grayCode = (licznik >> 1) ^ licznik;
        LATA = grayCode;
        licznik++;
        __delay32(1000000);  // Zmienna opó?niaj?ca dla p?ynnego liczenia
        uint16_t button = zmiana_przycisku();
        if(button) { numer_programu += button; break; }
    }
}
// 8 bitowy licznik w kodzie Graya zliczaj?cy w dó? (repr. 255...0)
void grayDOWN(){
    unsigned char licznik = 255;
    unsigned char grayCode;
    while(1) {
        grayCode = (licznik >> 1) ^ licznik;
        LATA = grayCode;
        licznik--;
        __delay32(1000000);  // Zmienna opó?niaj?ca dla p?ynnego liczenia
        uint16_t button = zmiana_przycisku();
        if(button) { numer_programu += button; break; }
    }
}
// 2x4 bitowy licznik w kodzie BCD zliczaj?cy w gór? (0...99)
void bcdUP(){
    unsigned char dziesiatki = 0;  // Pierwsza cyfra (dziesi?tki)
    unsigned char jednosci = 0;    // Druga cyfra (jednostki)
    while(1){
        LATA = (dziesiatki << 4) | jednosci;  // ??czenie obu cyfr w jedno s?owo
        __delay32(1000000);
         jednosci++;
        if (jednosci > 9) {
            jednosci = 0;  
            dziesiatki++; 
        }
        // Gdy liczba osi?gnie 99, wró? do 00
        if (dziesiatki > 9) {
            dziesiatki = 0;
        }
        uint16_t button = zmiana_przycisku();
        if(button) { numer_programu += button; break; }
    }
}
// 2x4 bitowy licznik w kodzie BCD zliczaj?cy w dó? (99...0)
void bcdDOWN() {
    unsigned char dziesiatki = 9;  // Pierwsza cyfra (dziesi?tki) zaczyna si? od 9
    unsigned char jednosci = 9;    // Druga cyfra (jednostki) zaczyna si? od 9

    while(1) {
        // Przesy?anie cyfr BCD do portów
        LATA = (dziesiatki << 4) | jednosci;  // ??czenie obu cyfr w jedno s?owo
        __delay32(1000000); 
        // Zmniejsz jednostki
        if (jednosci == 0) {
            jednosci = 9;  // Reset jednostek
            dziesiatki--;  // Zmniejsz dziesi?tki
        } else {
            jednosci--;
        }
        // Gdy liczba osi?gnie 00, wró? do 99
        if (dziesiatki == 0 && jednosci == 0) {
            dziesiatki = 9;  // Restart na 99
            jednosci = 9;
        }
        uint16_t button = zmiana_przycisku();
        if(button) { numer_programu += button; break; }
    }
}
// 3 bitowy wezyk poruszajacy sie lewo-prawo
void snake() {
    unsigned char wez = 0b00000111;  // Pocz?tkowy stan w??a
    uint16_t kierunek = 1;  // Kierunek ruchu: 1 - w prawo, -1 - w lewo
    while(1) {
        LATA = wez;  // Wy?wietl aktualny stan w??a na porcie (np. LED)

        __delay32(1000000);  // Zmienna opó?niaj?ca dla p?ynnego poruszania

        if (kierunek == 1) {
            wez <<= 1;  // Przesuni?cie w lewo
            if (wez > 0b01111000) {  // Po przekroczeniu 7 bitów (11100000), wracamy do 11100000
                wez = 0b11100000;  // Wracamy do stanu 11100000 (g?ówka na prawo)
                kierunek = -1;  // Zmieniamy kierunek na lewo
            }
        }
        else if (kierunek == -1) {
            wez >>= 1;  // Przesuni?cie w prawo
            if (wez < 0b00000111) {  // Po przekroczeniu 7 bitów (00000000), wracamy do 00000111
                wez = 0b00000111;  // Wracamy do stanu 00000111 (g?ówka na lewo)
                kierunek = 1;  // Zmieniamy kierunek na prawo
            }
        }
        uint16_t button = zmiana_przycisku();
        if(button) { numer_programu += button; break; }
    }
}
// Kolejka
// do poprawnienia fajne ale nie dziala jak ma 
void kolejka() {
    unsigned char kolejka = 0b00000000;  // Pocz?tkowa pustka w kolejce
    unsigned char nowy_bit = 1;          // Nowy bit, który b?dzie dodany do kolejki (np. 1 lub 0)
    while(1) {
        // Przesuni?cie bitów w lewo
        kolejka <<= 1;

        kolejka |= nowy_bit;  // Operacja OR ustawia najmniej znacz?cy bit na nowy bit
        LATA = kolejka;
        __delay32(1000000); 
        // Zmieniamy bit dodawany do kolejki, mo?e to by? zmienna zmieniaj?ca si? w czasie
        nowy_bit = (nowy_bit == 0) ? 1 : 0;  // Przyk?adowa zmiana bitu (np. naprzemiennie 0 i 1)
        // Je?li kolejka jest pe?na (8 bitów), nadpisujemy najstarszy bit
        if (kolejka == 0b11111111) {
            kolejka = 0b00000000;  // Zresetuj kolejk? do stanu pocz?tkowego (pe?na kolejka nadpisana)
        }
        uint16_t button = zmiana_przycisku();
        if(button) { numer_programu += button; break; }
    }
}

// 6 bitowy generator liczb pseudolosowych oparty o konfiguracj? 11100111

// G?ówna funkcja programu
int main(void) {
    init();
    while(1) {
        if(numer_programu < 1 || numer_programu > 7){
            numer_programu = 1;
        }
        else if(numer_programu == 1){
            binUP();
        }
        else if(numer_programu == 2){
            binDOWN();
        }
        else if(numer_programu == 3){
            grayUP();
        }
        else if(numer_programu == 4){
            grayDOWN();
        }
        else if(numer_programu == 5){
            bcdUP();
        }
        else if(numer_programu == 6){
            bcdDOWN();
        }
        else if(numer_programu == 7){
            snake();
        }
    }
}
