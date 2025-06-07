/*
 * File:   main.c
 * Author: Jakub Budzich - ISI1
 * 
 * Created on May 26, 2025, 8:30
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
#include <string.h>
#include "lcd.h"

// Deklaracja zegara systemowego
#define XTAL_FREQ 8000000
#define FCY 4000000

// DEKLARACJE FUNKCJI
void init_adc(void);
void czytaj_potencjometr(void);
void ustaw_urzadzenie(void);
void sprawdz_czas(void);
void pokaz_na_ekranie(void);
void resetuj_gre(void);

// Stany gry
#define STAN_WYBOR_CZASU 0      // wybieranie czasu gry
#define STAN_GRACZ1 1           // odmierza czas gracza 1
#define STAN_GRACZ2 2           // odmierza czas gracza 2  
#define STAN_KONIEC 4           // koniec gry

// Czasy gry (w sekundach)
uint16_t czasy_opcje[] = {300, 180, 60}; // 5min, 3min, 1min
uint8_t opcje_ilosc = 3;
char* nazwy_czasow[] = {"5 min", "3 min", "1 min"};

// Zmienne globalne
volatile uint16_t czas_gracz1 = 0;      // czas pozostaly graczowi 1 (sekundy)
volatile uint16_t czas_gracz2 = 0;      // czas pozostaly graczowi 2 (sekundy)
volatile uint8_t stan_gry = STAN_WYBOR_CZASU;
volatile uint8_t aktywny_gracz = 1;     // 1 lub 2
volatile uint8_t wybrana_opcja = 1;     // domyslnie 3 min
volatile uint16_t odswiez_ekran = 1;
volatile uint16_t licznik_ms = 0;
volatile uint16_t ostatnia_sekunda = 0;
volatile uint8_t zwyciezca = 0;         // 1 lub 2 - kto wygral

// ADC dla potencjometru
volatile uint16_t wartosc_potencjometru = 0;

// Funkcja przerwania timera
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void)
{
    IFS0bits.T1IF = 0;
    licznik_ms++;
    
    // Odczyt potencjometru co 100ms
    if (licznik_ms % 100 == 0 && stan_gry == STAN_WYBOR_CZASU) {
        czytaj_potencjometr();
    }
}

// Przerwanie Change Notification - obsluga przyciskow
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void) {
    __delay32(FCY/100);  // debouncing 10ms
    
    // Przycisk gracza 1 (RD6)
    if(PORTDbits.RD6 == 0) {
        if (stan_gry == STAN_GRACZ1) {
            // Gracz 1 skonczyl ruch - teraz kolej gracza 2
            stan_gry = STAN_GRACZ2;
            aktywny_gracz = 2;
            ostatnia_sekunda = licznik_ms;
            odswiez_ekran = 1;
        }
        // START GRY: Gracz 1 startuje czas graczowi 2
        else if (stan_gry == STAN_WYBOR_CZASU) {
            czas_gracz1 = czasy_opcje[wybrana_opcja];
            czas_gracz2 = czasy_opcje[wybrana_opcja];
            stan_gry = STAN_GRACZ2;
            aktywny_gracz = 2;
            ostatnia_sekunda = licznik_ms;
            odswiez_ekran = 1;
        }
        // RESTART PO KONCU GRY
        else if (stan_gry == STAN_KONIEC) {
            resetuj_gre();
        }
    }
    
    // Przycisk gracza 2 (RD13)
    else if(PORTDbits.RD13 == 0) {
        if (stan_gry == STAN_GRACZ2) {
            // Gracz 2 skonczyl ruch - teraz kolej gracza 1
            stan_gry = STAN_GRACZ1;
            aktywny_gracz = 1;
            ostatnia_sekunda = licznik_ms;
            odswiez_ekran = 1;
        }
        // START GRY: Gracz 2 startuje czas graczowi 1
        else if (stan_gry == STAN_WYBOR_CZASU) {
            czas_gracz1 = czasy_opcje[wybrana_opcja];
            czas_gracz2 = czasy_opcje[wybrana_opcja];
            stan_gry = STAN_GRACZ1;
            aktywny_gracz = 1;
            ostatnia_sekunda = licznik_ms;
            odswiez_ekran = 1;
        }
        // RESTART PO KONCU GRY
        else if (stan_gry == STAN_KONIEC) {
            resetuj_gre();
        }
    }
    
    // Czekaj na zwolnienie przyciskow
    while(PORTDbits.RD6 == 0 || PORTDbits.RD13 == 0);
    
    // Wyczysc flage przerwania
    IFS1bits.CNIF = 0;
}

// Inicjalizacja ADC
void init_adc(void) 
{
    AD1PCFGbits.PCFG5 = 0;    // AN5 jako wejscie analogowe
    AD1CON1 = 0x00E0;         
    AD1CON2 = 0;
    AD1CON3 = 0x1F3F;         
    AD1CHS = 5;               // Kanal AN5
    AD1CON1bits.ADON = 1;     // Wlacz ADC
}

// Odczyt potencjometru
void czytaj_potencjometr(void) 
{
    AD1CON1bits.SAMP = 1;
    __delay32(100);
    AD1CON1bits.SAMP = 0;
    while (!AD1CON1bits.DONE);
    wartosc_potencjometru = ADC1BUF0;
    
    // Przelicz na opcje czasu (3 opcje)
    uint8_t nowa_opcja = (wartosc_potencjometru * opcje_ilosc) / 1024;
    if (nowa_opcja >= opcje_ilosc) nowa_opcja = opcje_ilosc - 1;
    
    if (nowa_opcja != wybrana_opcja) {
        wybrana_opcja = nowa_opcja;
        odswiez_ekran = 1;
    }
}

int main(void) 
{
    ustaw_urzadzenie();
    
    while (1) {
        sprawdz_czas();
        
        if (odswiez_ekran) {
            pokaz_na_ekranie();
            odswiez_ekran = 0;
        }
        
        __delay32(1000);
    }
    
    return 0;
}

// Inicjalizacja urzadzenia
void ustaw_urzadzenie(void) 
{
    // Konfiguracja ADC - wszystkie cyfrowe oprocz AN5
    AD1PCFG = 0xFFDF;           
    TRISBbits.TRISB5 = 1;       // RB5/AN5 jako wejscie analogowe
    
    // Konfiguracja pinow jako wejscia
    TRISA = 0x0000;             // Port A jako wyjscie (dla LCD)
    TRISD = 0xFFFF;             // Port D jako wejscie (przyciski)
    
    // Konfiguracja Change Notification interrupts
    CNPU1bits.CN15PUE = 1;      // Pull-up dla RD6 (gracz 1)
    CNPU2bits.CN19PUE = 1;      // Pull-up dla RD13 (gracz 2)
    
    // Wlaczenie przerwan Change Notification
    CNEN1bits.CN15IE = 1;       // Wlacz przerwanie dla RD6
    CNEN2bits.CN19IE = 1;       // Wlacz przerwanie dla RD13
    
    IFS1bits.CNIF = 0;          // Wyczysc flage przerwania CN
    IEC1bits.CNIE = 1;          // Wlacz przerwania CN
    
    // Inicjalizacja LCD
    LCD_Initialize();
    LCD_ClearScreen();
    
    // Konfiguracja timera na 1ms
    T1CON = 0;
    TMR1 = 0;
    PR1 = FCY/1000 - 1;         // 1ms
    T1CONbits.TCKPS = 0b00;     
    IPC0bits.T1IP = 3;          // Priorytet przerwania 
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 1;
    T1CONbits.TON = 1;
    
    // Inicjalizacja ADC
    init_adc();
    
    // Wlacz przerwania globalne
    INTCON1bits.NSTDIS = 0;
    
    // domyslne czasy
    czas_gracz1 = czasy_opcje[wybrana_opcja];
    czas_gracz2 = czasy_opcje[wybrana_opcja];
}

// Sprawdzanie czasu
void sprawdz_czas(void) 
{
    if ((stan_gry == STAN_GRACZ1 || stan_gry == STAN_GRACZ2) && 
        licznik_ms - ostatnia_sekunda >= 1000) {
        
        ostatnia_sekunda = licznik_ms;
        
        if (stan_gry == STAN_GRACZ1 && czas_gracz1 > 0) {
            czas_gracz1--;
            if (czas_gracz1 == 0) {
                // Gracz 1 przegral przez czas
                stan_gry = STAN_KONIEC;
                zwyciezca = 2;
            }
        } else if (stan_gry == STAN_GRACZ2 && czas_gracz2 > 0) {
            czas_gracz2--;
            if (czas_gracz2 == 0) {
                // Gracz 2 przegral przez czas
                stan_gry = STAN_KONIEC;
                zwyciezca = 1;
            }
        }
        
        odswiez_ekran = 1;
    }
}

// Wyswietlanie na ekranie
void pokaz_na_ekranie(void) 
{
    char linia1[17], linia2[17];
    uint16_t min1, sek1, min2, sek2;
    
    LCD_ClearScreen();
    
    switch (stan_gry) {
        case STAN_WYBOR_CZASU:
            sprintf(linia1, "Wybierz czas:");     
            sprintf(linia2, "-> %s <-", nazwy_czasow[wybrana_opcja]);
            break;
            
        case STAN_GRACZ1:
        case STAN_GRACZ2:
            min1 = czas_gracz1 / 60;
            sek1 = czas_gracz1 % 60;
            min2 = czas_gracz2 / 60;
            sek2 = czas_gracz2 % 60;
            
            // Wyswietl gracza 1 z oznaczeniem aktywnosci
            if (stan_gry == STAN_GRACZ1) {
                sprintf(linia1, "*Gracz1 %02d:%02d", min1, sek1);
            } else {
                sprintf(linia1, " Gracz1 %02d:%02d", min1, sek1); 
            }
            
            // Wyswietl gracza 2 z oznaczeniem aktywnosci  
            if (stan_gry == STAN_GRACZ2) {
                sprintf(linia2, "*Gracz2 %02d:%02d", min2, sek2); 
            } else {
                sprintf(linia2, " Gracz2 %02d:%02d", min2, sek2); 
            }
            break;
            
        case STAN_KONIEC:
            sprintf(linia1, "KONIEC GRY!");    
            sprintf(linia2, "Wygral gracz %d", zwyciezca);
            break;
    }
    
    LCD_PutString(linia1, strlen(linia1));
    LCD_PutChar('\n');
    LCD_PutString(linia2, strlen(linia2));
}

// Reset gry
void resetuj_gre(void) 
{
    stan_gry = STAN_WYBOR_CZASU;
    aktywny_gracz = 1;
    zwyciezca = 0;
    czas_gracz1 = czasy_opcje[wybrana_opcja];
    czas_gracz2 = czasy_opcje[wybrana_opcja];
    odswiez_ekran = 1;
}
