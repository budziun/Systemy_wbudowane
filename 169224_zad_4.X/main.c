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
#define FCY 4000000  // Częstotliwość zegara instrukcji (FCY = FOSC/2)

#include <stdio.h>
#include <stdlib.h>
#include <xc.h>
#include <libpic30.h>  // Dla funkcji __delay_ms
#include "p24FJ128GA010.h"
#include "lcd.h"

// Definicje dla LCD, które mogą nie być widoczne
#define LCD_COMMAND_CLEAR_SCREEN        0x01
#define LCD_COMMAND_RETURN_HOME         0x02
#define LCD_COMMAND_ENTER_DATA_MODE     0x06
#define LCD_COMMAND_CURSOR_OFF          0x0C
#define LCD_COMMAND_CURSOR_ON           0x0F
#define LCD_COMMAND_MOVE_CURSOR_LEFT    0x10
#define LCD_COMMAND_MOVE_CURSOR_RIGHT   0x14
#define LCD_COMMAND_SET_MODE_4_BIT      0x28
#define LCD_COMMAND_SET_MODE_8_BIT      0x38
#define LCD_COMMAND_ROW_0_HOME          0x80
#define LCD_COMMAND_ROW_1_HOME          0xC0
#define LCD_F_INSTR                     (((SYSTEM_PERIPHERAL_CLOCK/1000)*40)/1000)/12

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
volatile uint32_t czasSystemowy = 0; // Licznik milisekund
volatile uint32_t ostatniCzasSekundy = 0; // Ostatni czas aktualizacji sekundy
volatile uint8_t odliczanieZakonczone = 0; // Flaga wskazująca na zakończenie odliczania

// Prototypy funkcji
void inicjalizujUrzadzenie(void);
void obslugaPrzyciskow(void);
void aktualizujWyswietlacz(void);
void rozpocznijOdliczanie(void);
void zatrzymajOdliczanie(void);
void resetujKuchenke(void);
void obslugaCzasu(void);

// Funkcja przerwania Timer1 - zwiększa licznik milisekund
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void)
{
    IFS0bits.T1IF = 0;  // Wyczyść flagę przerwania
    
    // Zwiększ licznik czasu systemowego (każde przerwanie to 1ms)
    czasSystemowy++;
    
    // Miganie dwukropka co 500ms
    if (czasSystemowy % 500 == 0) {
        mignijDwukropkiem = !mignijDwukropkiem;
        if (stanKuchenki == PAUZA) {
            odswiezWyswietlacz = 1;
        }
    }
}

int main(void) {
    inicjalizujUrzadzenie();
    
    // Główna pętla programu
    while (1) {
        obslugaPrzyciskow();
        obslugaCzasu();
        
        if (odswiezWyswietlacz) {
            aktualizujWyswietlacz();
            odswiezWyswietlacz = 0;
        }
    }
    
    return EXIT_SUCCESS;
}

// Obsługa upływu czasu
void obslugaCzasu(void) {
    // Jeśli kuchenka działa i minęło 1000ms, odlicz 1 sekundę
    if (stanKuchenki == DZIALA && pozostalyCzas > 0) {
        if (czasSystemowy - ostatniCzasSekundy >= 1000) {
            pozostalyCzas--;
            ostatniCzasSekundy = czasSystemowy;
            odswiezWyswietlacz = 1;
            
            // Koniec czasu
            if (pozostalyCzas == 0) {
                zatrzymajOdliczanie();
            }
        }
    }
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
    
    // Konfiguracja Timer1 (1 milisekunda) - POPRAWIONA
    T1CON = 0;              // Wyczyść rejestr kontrolny
    TMR1 = 0;               // Wyczyść licznik
    PR1 = FCY/1000;         // Ustawienie na 1ms (FCY = 4MHz)
    T1CONbits.TCKPS = 0b00; // Prescaler 1:1
    IPC0bits.T1IP = 3;      // Priorytet przerwania
    IFS0bits.T1IF = 0;      // Wyczyść flagę przerwania
    IEC0bits.T1IE = 1;      // Włącz przerwanie
    T1CONbits.TON = 1;      // Włącz timer
    
    // Wyświetl początkowy stan
    LCD_PutString("GOTOWE ZA:", 10); 
    LCD_PutChar('\n');      // Przejście do drugiej linii
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
    
    // Wyczyść ekran i wyświetl od początku
    LCD_ClearScreen();
    
    // Wyświetl stan kuchenki w pierwszej linii
    switch (stanKuchenki) {
        case ZATRZYMANA:
            if (pozostalyCzas == 0) {
                // Sprawdź, czy to jest stan po zakończeniu odliczania czy reset/inicjalizacja
                if (odliczanieZakonczone) {
                    // To zakończone odliczanie
                    LCD_PutString("GOTOWE", 6);
                } else {
                    // To stan początkowy lub reset
                    LCD_PutString("GOTOWE ZA:", 10);
                }
            } else {
                // Gdy czas jest nastawiony, ale kuchenka nie działa
                LCD_PutString("GOTOWE ZA:", 10);
            }
            break;
        case DZIALA:
            LCD_PutString("Pracuje", 7);
            // Gdy kuchenka działa, ustawiamy flagę, że może nastąpić zakończenie odliczania
            odliczanieZakonczone = 1;
            break;
        case PAUZA:
            LCD_PutString("Pauza", 5);
            break;
    }
    
    // Przejdź do drugiej linii
    LCD_PutChar('\n');
    
    // Wyświetl czas lub komunikat w drugiej linii
    if (stanKuchenki == ZATRZYMANA && pozostalyCzas == 0 && odliczanieZakonczone) {
        // Stan po zakończeniu odliczania
        LCD_PutString("SMACZNEGO!", 10);
    } else {
        // Wyświetl czas w drugiej linii
        if (stanKuchenki == PAUZA && mignijDwukropkiem) {
            sprintf(bufor, "Czas: %02d %02d", minuty, sekundy);
        } else {
            sprintf(bufor, "Czas: %02d:%02d", minuty, sekundy);
        }
        LCD_PutString(bufor, 11);
    }
}

void zatrzymajOdliczanie(void) {
    stanKuchenki = ZATRZYMANA;
    odswiezWyswietlacz = 1;
    
    // Wyświetl komunikat tylko po zakończeniu odliczania, a nie przy resecie
    if (pozostalyCzas == 0) {
        // Komunikat końcowy będzie wyświetlony w funkcji aktualizujWyswietlacz
    }
}

void resetujKuchenke(void) {
    pozostalyCzas = 0;
    stanKuchenki = ZATRZYMANA;
    
    // Resetujemy zmienną globalną
    odliczanieZakonczone = 0;
    
    odswiezWyswietlacz = 1;
}

void rozpocznijOdliczanie(void) {
    stanKuchenki = DZIALA;
    ostatniCzasSekundy = czasSystemowy; // Ustaw czas początkowy
    odswiezWyswietlacz = 1;
}
