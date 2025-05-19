/* 
 * File:   main.c
 * Author: Jakub Budzich - 169224
 *
 * Created on 19 maj 2025, 08:13
 * Modified for alarm control with potentiometer
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

// Definicje stanów alarmu
#define ALARM_OFF 0
#define ALARM_MRUGANIE 1
#define ALARM_WSZYSTKIE 2

volatile uint16_t wartosc_potencjometru = 0;  // wartosc odczytana z potencjometru
volatile uint16_t nastawa_alarmowa = 512;     // nastawa alarmowa (polowa zakresu 0-1023)
volatile uint8_t stan_alarmu = ALARM_OFF;     // aktualny stan alarmu
volatile uint32_t licznik_czasu = 0;          // licznik czasu do odmierzania 5 sekund
volatile uint32_t licznik_mrugania = 0;       // licznik do kontroli czestotliwosci mrugania
volatile uint8_t mruganie_stan = 0;           // stan mrugania diody (0 - zgaszona, 1 - zapalona)

// parametry do zarzadzania mruganiem jednej diody i czasem gdy wszystkie sie zaswieca
#define CZAS_MRUGANIA 450      // calkowity czas fazy mrugania (ilosc iteracji)
                               // warto?? 450 daje 5 sekund przy delay(25) w main 
#define CZESTOTL_MRUGANIA 40   // co ile iteracji zmienic stan diody


// Funkcja opoznienia
void delay(uint32_t czas) {
    uint32_t i, j;
    for(i = 0; i < czas; i++) {
        for(j = 0; j < 100; j++) {
            asm("NOP");
        }
    }
}

// Inicjalizacja ADC do odczytu potencjometru
void initADC() {
    // Konfiguracja portu analogowego
    AD1PCFGbits.PCFG5 = 0;    // AN5 jako wejscie analogowe
    AD1CON1 = 0x00E0;         // SSRC = 111 zako?cz konwersje na samym koncu
    AD1CON2 = 0;
    AD1CON3 = 0x1F3F;         // Czas probkowania = 31Tad, Tad = 64Tcy
    AD1CHS = 5;               // Wybor kana?u AN5 (potencjometr)
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
    TRISA = 0x0000;           // Port A jako wyjscie
    TRISD = 0xFFFF;           // Port D jako wejscie
    
    // Konfiguracja przerwan od pinow
    CNPU1bits.CN15PUE = 1;    // Pull-up dla RD6 (przycisk wylaczenia alarmu)
    
    // Wlaczenie przerwan dla przyciskow
    CNEN1bits.CN15IE = 1;     // Wlacz przerwanie dla RD6
    
    IFS1bits.CNIF = 0;        // Wyczysc flage przerwania CN
    IEC1bits.CNIE = 1;        // Wlacz przerwania CN
    
    // Inicjalizacja ADC
    initADC();
    
    // Wszystkie diody poczatkowo wylaczone
    LATA = 0x0000;
}

// Procedura obs?ugi przerwania przyciskami 
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void) {
    __delay32(200);
    
    // Sprawdzenie, czy przycisk RD6 zostal nacisniety (wylaczenie alarmu)
    if(PORTDbits.RD6 == 0) {
        stan_alarmu = ALARM_OFF;
        LATA = 0x0000;  // Wylacz wszystkie diody
    }
    
    while(PORTDbits.RD6 == 0);  // Czekaj na zwolnienie przycisku
    
    // Wyczyszczenie flagi przerwania
    IFS1bits.CNIF = 0;
}

void alarm() {
    // Odczyt wartosci z potencjometru
    wartosc_potencjometru = czytajPotencjometr();
    
    // Sprawdzenie warunkow alarmu
    switch(stan_alarmu) {
        case ALARM_OFF:
            // Jesli wartosc przekroczyla nastawe, uruchom alarm (mruganie jednej diody)
            if(wartosc_potencjometru > nastawa_alarmowa) {
                stan_alarmu = ALARM_MRUGANIE;
                licznik_czasu = 0;       // Zresetuj licznik czasu
                licznik_mrugania = 0;    // Zresetuj licznik mrugania
                mruganie_stan = 0;      // Rozpocznij od zgaszonej diody
            }
            break;
            
        case ALARM_MRUGANIE:
            // Zwieksz licznik czasu
            licznik_czasu++;
            licznik_mrugania++;
            
            // Mruganie jedna dioda co CZESTOTL_MRUGANIA cykli
            if(licznik_mrugania >= CZESTOTL_MRUGANIA) {
                licznik_mrugania = 0;
                
                if(mruganie_stan == 0) {
                    LATA = 0x0001;  // Zapal pierwsz? diode
                    mruganie_stan = 1;
                } else {
                    LATA = 0x0000;  // Zgas wszystkie diody
                    mruganie_stan = 0;
                }
            }
            
            // Po czasie okreslonym przez CZAS_MRUGANIA przejdz do stanu ALARM_WSZYSTKIE
            if(licznik_czasu >= CZAS_MRUGANIA) {
                stan_alarmu = ALARM_WSZYSTKIE;
            }
            
            // Jesli wartosc spadla ponizej nastawy, wylacz alarm
            if(wartosc_potencjometru < nastawa_alarmowa) {
                stan_alarmu = ALARM_OFF;
                LATA = 0x0000;  // Wylacz wszystkie diody
            }
            break;
            
        case ALARM_WSZYSTKIE:
            LATA = 0x00FF;  // Zapal wszystkie diody 
            
            // Jesli wartocs spadla ponizej nastawy, wylacz alarm
            if(wartosc_potencjometru < nastawa_alarmowa) {
                stan_alarmu = ALARM_OFF;
                LATA = 0x0000;  // Wylacz wszystkie diody
            }
            break;
    }
}

// Glowna funkcja programu
int main(void) {
    init();
    
    while(1) {
        alarm();
        
        delay(25);
    }
    
    return 0;
}