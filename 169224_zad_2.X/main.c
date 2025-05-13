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
volatile uint8_t flaga = 0; // flaga informujaca o zmianie programu
volatile uint16_t predkosc = 0; // wartosc potencjometru

// Funkcja opoznienie z regulacja predkosci
void delay(uint32_t podstawa) {
    uint32_t ilosc, i, j;
    
    // Przeliczenie opoznienia na podstawie odczytu z potencjometru
    // predkosc bedzie w zakresie 0-1023 (10-bitowy ADC)
    // i dzielmy na 5 rozncyh poziomow predkosci
    // 5 roznych predkosci bez zachowanego porządku aby było widoczne że istnieje 5 róźnych stanów
    if (predkosc < 205)         ilosc = podstawa /2;  // szybko
    else if (predkosc < 410)    ilosc = podstawa * 4;  // wolno
    else if (predkosc < 615)    ilosc = podstawa /4;  // jeszcze szybciej niz przy 205 wartosci
    else if (predkosc < 820)    ilosc = podstawa * 12;  // najwolniej 
    else                        ilosc = podstawa /8;  // najszybiciej

    for(i = 0; i < ilosc; i++) {
        for(j = 0; j < 100; j++) {
            asm("NOP");
        }
    }
}

// Inicjalizacja ADC do odczytu potencjometru
void initADC() {
    // Konfiguracja portu analogowego
    AD1PCFGbits.PCFG5 = 0;    // AN5 jako wejscie analogowe
    AD1CON1 = 0x00E0;         // SSRC = 111 zakoncz konwersje na samym koncu
    AD1CON2 = 0;
    AD1CON3 = 0x1F3F;         // Czas probkowania = 31Tad, Tad = 64Tcy
    AD1CHS = 5;               // Wybor kanalu AN5 (potencjometr)
    AD1CON1bits.ADON = 1;     // Wlacz modul ADC
}

// Funkcja do odczytu wartosci potencjometru
uint16_t czytajPotencjometr() {
    AD1CON1bits.SAMP = 1;     // Rozpocznij probkowanie
    __delay32(100);           // Daj czas na probkowanie
    AD1CON1bits.SAMP = 0;     // Rozpocznij konwersje
    while (!AD1CON1bits.DONE); // Czekaj na zakonczenie konwersji
    return ADC1BUF0;          // Zwroc wynik
}

// Inicjalizacja portow i przerwan
void init() {
    AD1PCFG = 0xFFDF;         // Wszystkie piny cyfrowe oprocz AN5
    TRISA = 0x0000;           // Port A jako wyj?cie
    TRISD = 0xFFFF;           // Port D jako wej?cie
    
    // Konfiguracja przerwan od pinow
    CNPU2bits.CN19PUE = 1;    // Pull-up dla RD13
    CNPU1bits.CN15PUE = 1;    // Pull-up dla RD6 
    
    // Wlaczenie przerwan dla przyciskow
    CNEN1bits.CN15IE = 1;     // Wlacz przerwanie dla RD6
    CNEN2bits.CN19IE = 1;     // Wlacz przerwanie dla RD13
    
    IFS1bits.CNIF = 0;        // Wyczysc flage przerwania CN
    IEC1bits.CNIE = 1;        // Wlacz przerwania CN
    
    // Inicjalizacja ADC
    initADC();
}

// Procedura obslugi przerwania przyciskami 
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void) {
    __delay32(200);
    // Sprawdzenie, ktory przycisk zostal nacisniety
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
    // Wyczysczenie flagi
    IFS1bits.CNIF = 0;
}

// 1. Snake wezyk od prawej do lewej
void snake() {
    unsigned char wez = 0b00000111;
    uint16_t kierunek = 1;
    flaga = 0;
    
    while(!flaga) {
        // Odczyt potencjometru przed kazda iteracja
        predkosc = czytajPotencjometr();
        
        LATA = wez;
        delay(150); //delay okreslany wartoscia z potencjometru 
        
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
                wez = 0b00000111 << 1;
                kierunek = 1;
            }
        }
    }
}

// 2. licznik wartosci binarnych w dol
void binDown(){
    unsigned char licznik = 255;
    flaga = 0;
    while(!flaga) {
        // program odczytuje przed kazda iteracja wartosc z potencjometru
        predkosc = czytajPotencjometr();
        
        LATA = licznik--;
        delay(150); //delay okreslany wartoscia z potencjometru 
    }
}
// Glowna funkcja programu z wyborem programu
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
