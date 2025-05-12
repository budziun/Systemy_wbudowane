/* 
 * File:   main.c
 * Author: Jakub Budzich - 169224
 *
 * Created on 12 maj 2025, 08:17
 * Modified for potentiometer control
 */
#pragma config POSCMOD = NONE      // Primary Oscillator Select
#pragma config OSCIOFNC = ON       // Primary Oscillator Output Function
#pragma config FCKSM = CSDCMD      // Clock Switching and Monitor
#pragma config FNOSC = FRC         // Oscillator Select
#pragma config IESO = OFF          // Internal External Switch Over Mode

#pragma config WDTPS = PS32768     // Watchdog Timer Postscaler
#pragma config FWPSA = PR128       // WDT Prescaler
#pragma config WINDIS = ON         // Watchdog Timer Window
#pragma config FWDTEN = OFF        // Watchdog Timer Enable
#pragma config ICS = PGx2          // Comm Channel Select
#pragma config GWRP = OFF          // General Code Segment Write Protect
#pragma config GCP = OFF           // General Code Segment Code Protect
#pragma config JTAGEN = OFF        // JTAG Port Enable

#define _XTAL_FREQ 8000000
#include <xc.h>
#include <libpic30.h>
#include <stdlib.h>
#include "p24FJ128GA010.h"

volatile uint16_t numer_programu = 1;
volatile uint8_t flaga = 0; // flaga informuj?ca o zmianie programu
volatile uint16_t predkosc = 0; // warto?? potencjometru

// Funkcja opó?nienie z regulacj? pr?dko?ci
void delay(uint32_t podstawa) {
    uint32_t ilosc, i, j;
    
    // Przeliczenie opó?nienia na podstawie odczytu z potencjometru
    // predkosc b?dzie w zakresie 0-1023 (10-bitowy ADC)
    // Dzielimy na 5 poziomów pr?dko?ci
    if (predkosc < 205)         ilosc = podstawa * 5;  // bardzo wolno
    else if (predkosc < 410)    ilosc = podstawa * 4;  // wolno
    else if (predkosc < 615)    ilosc = podstawa * 3;  // ?rednio
    else if (predkosc < 820)    ilosc = podstawa * 2;  // szybko
    else                        ilosc = podstawa;      // bardzo szybko

    for(i = 0; i < ilosc; i++) {
        for(j = 0; j < 100; j++) {
            asm("NOP");
        }
    }
}

// Inicjalizacja ADC do odczytu potencjometru
void initADC() {
    // Konfiguracja portu analogowego
    AD1PCFGbits.PCFG5 = 0;    // AN5 jako wej?cie analogowe
    AD1CON1 = 0x00E0;         // SSRC = 111 zako?cz konwersj? na samym ko?cu
    AD1CON2 = 0;
    AD1CON3 = 0x1F3F;         // Czas próbkowania = 31Tad, Tad = 64Tcy
    AD1CHS = 5;               // Wybór kana?u AN5 (potencjometr)
    AD1CON1bits.ADON = 1;     // W??cz modu? ADC
}

// Funkcja do odczytu warto?ci potencjometru
uint16_t czytajPotencjometr() {
    AD1CON1bits.SAMP = 1;     // Rozpocznij próbkowanie
    __delay32(100);           // Daj czas na próbkowanie
    AD1CON1bits.SAMP = 0;     // Rozpocznij konwersj?
    while (!AD1CON1bits.DONE); // Czekaj na zako?czenie konwersji
    return ADC1BUF0;          // Zwró? wynik
}

// Inicjalizacja portów i przerwa?
void init() {
    AD1PCFG = 0xFFDF;         // Wszystkie piny cyfrowe oprócz AN5
    TRISA = 0x0000;           // Port A jako wyj?cie
    TRISD = 0xFFFF;           // Port D jako wej?cie
    
    // Konfiguracja przerwa? od pinów
    CNPU2bits.CN19PUE = 1;    // Pull-up dla RD13
    CNPU1bits.CN15PUE = 1;    // Pull-up dla RD6 
    
    // W??czenie przerwa? Change Notification dla przycisków
    CNEN1bits.CN15IE = 1;     // W??cz przerwanie dla RD6
    CNEN2bits.CN19IE = 1;     // W??cz przerwanie dla RD13
    
    IFS1bits.CNIF = 0;        // Wyczy?? flag? przerwania CN
    IEC1bits.CNIE = 1;        // W??cz przerwania CN
    
    // Inicjalizacja ADC
    initADC();
}

// Procedura obs?ugi przerwania przyciskami 
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

// Program 1: W??yk poruszaj?cy si? lewo-prawo (wybrany jako pierwszy program, inny ni? liczniki)
void snake() {
    unsigned char wez = 0b00000111;
    uint16_t kierunek = 1;
    flaga = 0;
    
    while(!flaga) {
        // Odczyt potencjometru przed ka?d? iteracj?
        predkosc = czytajPotencjometr();
        
        LATA = wez;
        delay(150); // Podstawowe opó?nienie, modyfikowane przez funkcj? delay
        
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

// Program 2: licznik wartosci binarnych w dó?
void binDown(){
    unsigned char licznik = 255;
    flaga = 0;
    while(!flaga) {
        // program odczytuje przed ka?d? iteracj? warto?? potencjometru
        predkosc = czytajPotencjometr();
        
        LATA = licznik--;
        delay(150);
    }
}
// G?ówna funkcja programu z wyborem programu
int main(void) {
    init();
    
    while(1) {
        if(numer_programu < 1) {
            numer_programu = 2;
        } else if(numer_programu > 2) {
            numer_programu = 1;
        }
        switch(numer_programu) {
            case 1:
                snake();
                break;
            case 2:
                binDown(); 
                break;
        }
    }
    return 0;
}