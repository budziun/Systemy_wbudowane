/*
 * File:   main.c
 * Author: Jakub Budzich - ISI1
 *
 * Created on May 19, 2025, 9:14
 */

#pragma config POSCMOD = NONE
#pragma config OSCIOFNC = ON
#pragma config FCKSM = CSDCMD
#pragma config FNOSC = FRC
#pragma config IESO = OFF
#pragma config WDTPS = PS32768
#pragma config FWPSA = PR128
#pragma config WINDIS = ON
#pragma config FWDTEN = OFF
#pragma config ICS = PGx2
#pragma config GWRP = OFF
#pragma config GCP = OFF
#pragma config JTAGEN = OFF

#include <stdio.h>
#include <stdlib.h>
#include <xc.h>
#include <libpic30.h>
#include "lcd.h"

// Deklaracja zegara systemowego
#define XTAL_FREQ 8000000
#define FCY 4000000

// DEKLARACJE FUNKCJI - DODANE
void sprawdz_czas(void);
void ustaw_urzadzenie(void);
void pokaz_na_ekranie(void);
void zatrzymaj(void);
void zacznij(void);

// Zmienne globalne - volatile bo u?ywane w przerwaniach
volatile uint16_t czas_sekundy = 0;           // ile sekund zostalo
volatile uint8_t stan = 0;                    // 0=stop, 1=dziala, 2=pauza
volatile uint16_t odswiez_ekran = 1;          // czy odswiezyc wyswietlacz
volatile uint16_t migaj = 0;                  // do migania dwukropka
volatile uint16_t licznik_ms = 0;             // licznik milisekund
volatile uint16_t ostatnia_sekunda = 0;       // kiedy ostatnio odliczylismy sekunde
volatile uint16_t skonczyl = 0;               // czy skonczylo sie odliczanie

// Przerwanie od Timer1 (1ms)
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void)
{
    IFS0bits.T1IF = 0;          // wyczysc flage przerwania
    
    licznik_ms++;               // zwieksz licznik milisekund
    
    // Co 500ms zmien miganie
    if (licznik_ms % 500 == 0) {
        migaj = !migaj;        
    }
}

// Przerwanie Change Notification - obsluga przyciskow
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void) {
    __delay32(FCY/100);  // POPRAWIONE - debouncing 10ms
    
    // Przycisk +1min (RD6/CN15)
    if(PORTDbits.RD6 == 0) {
        czas_sekundy += 60;     // dodaj 60 sekund = 1 min
        if (czas_sekundy > 5999) czas_sekundy = 5999; // max 99:59
        odswiez_ekran = 1;      // odswiez ekran
    }
    
    // Przycisk +10sec (RD7/CN16) 
    else if(PORTDbits.RD7 == 0) {
        czas_sekundy += 10;     // dodaj 10 sekund
        if (czas_sekundy > 5999) czas_sekundy = 5999; // max 99:59
        odswiez_ekran = 1;      // odswiez ekran
    }
    
    // Przycisk Start/Stop (RD13/CN19)
    else if(PORTDbits.RD13 == 0) {
        if (stan == 0 || stan == 2) {      // jesli zatrzymana lub pauza
            if (czas_sekundy > 0) {         // jesli jest czas do odliczenia
                zacznij();                  // zacznij odliczanie
            }
        } else if (stan == 1) {             // jesli dziala
            stan = 2;                       // ustaw na pauze
            odswiez_ekran = 1;             // odswiez ekran
        }
    }
    
    // Czekaj na zwolnienie przycisków
    while(PORTDbits.RD6 == 0 || PORTDbits.RD7 == 0 || PORTDbits.RD13 == 0);
    
    // Wyczysc flage przerwania
    IFS1bits.CNIF = 0;
}

int main(void) 
{
    ustaw_urzadzenie();
    
    while (1) {
        sprawdz_czas();         // sprawdz czy minela sekunda
        
        if (odswiez_ekran) {    // jesli trzeba odswiezyc
            pokaz_na_ekranie();  // pokaz aktualny stan
            odswiez_ekran = 0; 
        }
        
        // Automatyczne resetowanie po 5 sekundach od zako?czenia
        static uint16_t czas_gotowe = 0;
        if (czas_sekundy == 0 && stan == 0 && skonczyl) {
            czas_gotowe++;
            if (czas_gotowe > 5000) {  // po 5 sekundach (5000ms)
                skonczyl = 0;
                czas_gotowe = 0;
                odswiez_ekran = 1;
            }
        } else {
            czas_gotowe = 0;
        }
    }
    
    return 0;
}

// Sprawdza czy minela sekunda i odlicza czas
void sprawdz_czas(void) 
{
    // Jesli kuchenka dziala i jest czas do odliczenia
    if (stan == 1 && czas_sekundy > 0) {
        // Jesli minelo 1000ms (1 sekunda)
        if (licznik_ms - ostatnia_sekunda >= 1000) {
            czas_sekundy--;                 // odlicz sekunde
            ostatnia_sekunda = licznik_ms;  // zapamietaj kiedy
            odswiez_ekran = 1;             // odswiez ekran
            
            // Jesli czas sie skonczyl
            if (czas_sekundy == 0) {
                zatrzymaj();                // zatrzymaj kuchenke
            }
        }
    }
}

// Inicjalizacja urzadzenia
void ustaw_urzadzenie(void) 
{
    AD1PCFG = 0xFFFF;           // wszystkie piny cyfrowe
    TRISA = 0x0000;             // Port A jako wyj?cie (dla LCD)
    TRISD = 0xFFFF;             // Port D jako wej?cie (przyciski)
    
    // Konfiguracja Change Notification interrupts
    
    CNPU1bits.CN15PUE = 1;      // Pull-up dla RD6 (+1min)
    CNPU2bits.CN16PUE = 1;      // Pull-up dla RD7 (+10sec)
    CNPU2bits.CN19PUE = 1;      // Pull-up dla RD13 (Start/Stop)
    
    // W??czenie przerwa? Change Notification
    CNEN1bits.CN15IE = 1;       // Wlacz przerwanie dla RD6
    CNEN2bits.CN16IE = 1;       // Wlacz przerwanie dla RD7
    CNEN2bits.CN19IE = 1;       // Wlacz przerwanie dla RD13
    
    IFS1bits.CNIF = 0;          // Wyczysc flage przerwania CN
    IEC1bits.CNIE = 1;          // Wlacz przerwania CN
    
    // Uruchomienie LCD
    LCD_Initialize();
    LCD_ClearScreen();
    
    // Ustaw timer na 1ms
    T1CON = 0;                  // wyczysc ustawienia timera
    TMR1 = 0;                   // wyczysc licznik
    PR1 = FCY/1000 - 1;         // POPRAWIONE - ustaw na 1ms
    T1CONbits.TCKPS = 0b00;     // bez dzielnika
    IPC0bits.T1IP = 3;          // priorytet 3 (nizszy niz CN)
    IFS0bits.T1IF = 0;          // wyczysc flage
    IEC0bits.T1IE = 1;          // wlacz przerwanie
    T1CONbits.TON = 1;          // wlacz timer
    
    // Tekst poczatkowy
    LCD_PutString("GOTOWE ZA:", 10);
    LCD_PutChar('\n');         
    LCD_PutString("Czas: 00:00", 11);
    
    // W??cz przerwania globalne
    INTCON1bits.NSTDIS = 0;
}

// Pokazuje aktualny stan na wyswietlaczu
void pokaz_na_ekranie(void) 
{
    char tekst[17];                   
    uint16_t minuty = czas_sekundy / 60;     // ile minut
    uint16_t sekundy = czas_sekundy % 60;    // ile sekund
    
    LCD_ClearScreen();
    
    if (stan == 0) {                    // zatrzymana
        if (czas_sekundy == 0) {
            if (skonczyl) {             // po zakonczeniu gotowania
                LCD_PutString("GOTOWE", 6);
            } else {                    // stan poczatkowy
                LCD_PutString("GOTOWE ZA:", 10); 
            }
        } else {
            LCD_PutString("GOTOWE ZA:", 10);  // ustawiono czas, ale nie wystartowano
        }
    } else if (stan == 1) {             // dziala
        LCD_PutString("Pracuje", 7);
        skonczyl = 1;                   
    } else if (stan == 2) {             // pauza
        LCD_PutString("Pauza", 5);
    }
    
    LCD_PutChar('\n');
    
    // Drugi wiersz
    if (stan == 0 && czas_sekundy == 0 && skonczyl) {
        LCD_PutString("SMACZNEGO!", 10);
    } else {
        // W trybie pauzy migaj dwukropkiem
        if (stan == 2 && migaj) {
            sprintf(tekst, "Czas: %02d %02d", minuty, sekundy);
        } else {
            sprintf(tekst, "Czas: %02d:%02d", minuty, sekundy);
        }
        LCD_PutString(tekst, 11);
    }
}

// Zatrzymanie odliczania
void zatrzymaj(void) 
{
    stan = 0;                   // ustaw stan na zatrzymana
    odswiez_ekran = 1;         // odswiez ekran
}

// Zaczyna odliczanie
void zacznij(void) 
{
    stan = 1;                           // stan - dziala
    ostatnia_sekunda = licznik_ms;      // zapamietaj czas startu
    odswiez_ekran = 1;                 // odswiez ekran
}