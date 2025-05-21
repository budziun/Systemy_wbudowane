/*
 * File:   main.c
 * Author: Jakub Budzich - ISI1
 *
 * Created on May 19, 2025, 9:23 AM
 */

// Konfiguracja
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

#define XTAL_FREQ 8000000

#include <stdio.h>
#include <stdlib.h>
#include <xc.h>
#include "p24FJ128GA010.h"
#include "lcd.h"

// Definicje pinów
#define BTN_ADD_1MIN PORTDbits.RD6
#define BTN_ADD_10SEC PORTDbits.RD7
#define BTN_START_STOP PORTAbits.RA7
#define BTN_RESET PORTDbits.RD13

// Stany kuchenki
typedef enum {
    ZATRZYMANA,
    DZIALA,
    PAUZA
} StanKuchenki;

// Zmienne globalne
volatile uint16_t pozostalyCzas = 0;  // Czas w sekundach
volatile StanKuchenki stanKuchenki = ZATRZYMANA;
volatile uint8_t odswiezWyswietlacz = 1;
volatile uint8_t mignijDwukropkiem = 0;

// Prototypy funkcji
void inicjalizujUrzadzenie(void);
void obslugaPrzyciskow(void);
void aktualizujWyswietlacz(void);
void obslugaTimera(void);
void wyswietlCzas(uint16_t czas);
void rozpocznijOdliczanie(void);
void zatrzymajOdliczanie(void);
void resetujKuchenke(void);

// Funkcja przerwania Timer1
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void)
{
    IFS0bits.T1IF = 0;  // Wyczyść flagę przerwania
    
    // Odliczanie czasu
    if (stanKuchenki == DZIALA && pozostalyCzas > 0) {
        pozostalyCzas--;
        odswiezWyswietlacz = 1;
        
        // Koniec czasu
        if (pozostalyCzas == 0) {
            zatrzymajOdliczanie();
        }
    }
    
    // Miganie dwukropkiem przy pauzie
    mignijDwukropkiem = !mignijDwukropkiem;
}

int main(void) {
    inicjalizujUrzadzenie();
    
    // Główna pętla programu
    while (1) {
        obslugaPrzyciskow();
        
        if (odswiezWyswietlacz) {
            aktualizujWyswietlacz();
            odswiezWyswietlacz = 0;
        }
    }
    
    return EXIT_SUCCESS;
}

void inicjalizujUrzadzenie(void) {
    // Konfiguracja pinów
    TRISDbits.TRISD6 = 1;   // RD6 jako wejście (dodaj 1min)
    TRISDbits.TRISD7 = 1;   // RD7 jako wejście (dodaj 10s)
    TRISAbits.TRISA7 = 1;   // RA7 jako wejście (start/stop)
    TRISDbits.TRISD13 = 1;  // RD13 jako wejście (reset)
    
    // Inicjalizacja LCD
    LCD_Initialize();
    LCD_ClearScreen();
    
    // Konfiguracja Timer1 (1 sekunda)
    T1CON = 0;              // Wyczyść rejestr kontrolny
    TMR1 = 0;               // Wyczyść licznik
    PR1 = 31250;            // Okres = (PR1 + 1) * prescaler * TCY = 31250 * 256 * (1/8MHz) = 1s
    T1CONbits.TCKPS = 0b11; // Prescaler 1:256
    IPC0bits.T1IP = 3;      // Priorytet przerwania
    IFS0bits.T1IF = 0;      // Wyczyść flagę przerwania
    IEC0bits.T1IE = 1;      // Włącz przerwanie
    
    // Wyświetl początkowy stan
    LCD_PutString("Kuchenka mikrofal", 16);
    LCD_SendCommand(LCD_COMMAND_ROW_1_HOME, LCD_F_INSTR);
    LCD_PutString("Czas: 00:00", 11);
    
    // Włącz przerwania
    INTCON1bits.NSTDIS = 0; // Włącz zagnieżdżone przerwania
}

void obslugaPrzyciskow(void) {
    static uint8_t poprzedniStanRD6 = 1;
    static uint8_t poprzedniStanRD7 = 1;
    static uint8_t poprzedniStanRA7 = 1;
    static uint8_t poprzedniStanRD13 = 1;
    
    // Obsługa przycisku dodającego 1 minutę
    if (poprzedniStanRD6 == 1 && BTN_ADD_1MIN == 0) {
        pozostalyCzas += 60;  // Dodaj 60 sekund
        odswiezWyswietlacz = 1;
    }
    poprzedniStanRD6 = BTN_ADD_1MIN;
    
    // Obsługa przycisku dodającego 10 sekund
    if (poprzedniStanRD7 == 1 && BTN_ADD_10SEC == 0) {
        pozostalyCzas += 10;  // Dodaj 10 sekund
        odswiezWyswietlacz = 1;
    }
    poprzedniStanRD7 = BTN_ADD_10SEC;
    
    // Obsługa przycisku Start/Stop
    if (poprzedniStanRA7 == 1 && BTN_START_STOP == 0) {
        if (stanKuchenki == ZATRZYMANA || stanKuchenki == PAUZA) {
            if (pozostalyCzas > 0) {
                rozpocznijOdliczanie();
            }
        } else {
            stanKuchenki = PAUZA;
            T1CONbits.TON = 0;  // Zatrzymanie timera
            odswiezWyswietlacz = 1;
        }
    }
    poprzedniStanRA7 = BTN_START_STOP;
    
    // Obsługa przycisku Reset
    if (poprzedniStanRD13 == 1 && BTN_RESET == 0) {
        resetujKuchenke();
    }
    poprzedniStanRD13 = BTN_RESET;
}

void aktualizujWyswietlacz(void) {
    char bufor[17];
    uint8_t minuty = pozostalyCzas / 60;
    uint8_t sekundy = pozostalyCzas % 60;
    
    // Wyświetl stan kuchenki w pierwszej linii
    LCD_SendCommand(LCD_COMMAND_ROW_0_HOME, LCD_F_INSTR);
    
    switch (stanKuchenki) {
        case ZATRZYMANA:
            LCD_PutString("Gotowa          ", 16);
            break;
        case DZIALA:
            LCD_PutString("Pracuje         ", 16);
            break;
        case PAUZA:
            LCD_PutString("Pauza           ", 16);
            break;
    }
    
    // Wyświetl czas w drugiej linii
    LCD_SendCommand(LCD_COMMAND_ROW_1_HOME, LCD_F_INSTR);
    
    if (stanKuchenki == PAUZA && mignijDwukropkiem) {
        sprintf(bufor, "Czas: %02d %02d    ", minuty, sekundy);
    } else {
        sprintf(bufor, "Czas: %02d:%02d    ", minuty, sekundy);
    }
    
    LCD_PutString(bufor, 16);
}

void rozpocznijOdliczanie(void) {
    stanKuchenki = DZIALA;
    T1CONbits.TON = 1;  // Uruchomienie timera
    odswiezWyswietlacz = 1;
}

void zatrzymajOdliczanie(void) {
    stanKuchenki = ZATRZYMANA;
    T1CONbits.TON = 0;  // Zatrzymanie timera
    odswiezWyswietlacz = 1;
    
    // Sygnalizacja zakończenia
    LCD_SendCommand(LCD_COMMAND_ROW_0_HOME, LCD_F_INSTR);
    LCD_PutString("Koniec!         ", 16);
}

void resetujKuchenke(void) {
    pozostalyCzas = 0;
    stanKuchenki = ZATRZYMANA;
    T1CONbits.TON = 0;  // Zatrzymanie timera
    odswiezWyswietlacz = 1;
}
