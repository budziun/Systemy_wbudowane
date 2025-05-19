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

volatile uint16_t wartosc_potencjometru = 0;  // warto?? odczytana z potencjometru
volatile uint16_t nastawa_alarmowa = 512;     // nastawa alarmowa (po?owa zakresu 0-1023)
volatile uint8_t stan_alarmu = ALARM_OFF;     // aktualny stan alarmu
volatile uint32_t licznik_czasu = 0;          // licznik czasu do odmierzania 5 sekund
volatile uint32_t licznik_mrugania = 0;       // licznik do kontroli cz?stotliwo?ci mrugania
volatile uint8_t mruganie_stan = 0;           // stan mrugania diody (0 - zgaszona, 1 - zapalona)

// parametry do zarz?dzania mruganiem jednej diody i czasem gdy wszystkie sie zaswieca
#define CZAS_MRUGANIA 450      // ca?kowity czas fazy mrugania (ilo?? iteracji)
                               // warto?? 450 daje 5 sekund przy delay(25) w main 
#define CZESTOTL_MRUGANIA 40   // co ile iteracji zmieni? stan diody


// Funkcja opó?nienia
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
    CNPU1bits.CN15PUE = 1;    // Pull-up dla RD6 (przycisk wy??czenia alarmu)
    
    // W??czenie przerwa? dla przycisków
    CNEN1bits.CN15IE = 1;     // W??cz przerwanie dla RD6
    
    IFS1bits.CNIF = 0;        // Wyczy?? flag? przerwania CN
    IEC1bits.CNIE = 1;        // W??cz przerwania CN
    
    // Inicjalizacja ADC
    initADC();
    
    // Wszystkie diody pocz?tkowo wy??czone
    LATA = 0x0000;
}

// Procedura obs?ugi przerwania przyciskami 
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void) {
    __delay32(200);  // Opó?nienie dla eliminacji drga? styków
    
    // Sprawdzenie, czy przycisk RD6 zosta? naci?ni?ty (wy??czenie alarmu)
    if(PORTDbits.RD6 == 0) {
        stan_alarmu = ALARM_OFF;
        LATA = 0x0000;  // Wy??cz wszystkie diody
    }
    
    while(PORTDbits.RD6 == 0);  // Czekaj na zwolnienie przycisku
    
    // Wyczyszczenie flagi przerwania
    IFS1bits.CNIF = 0;
}

void alarm() {
    // Odczyt warto?ci z potencjometru
    wartosc_potencjometru = czytajPotencjometr();
    
    // Sprawdzenie warunków alarmu
    switch(stan_alarmu) {
        case ALARM_OFF:
            // Je?li warto?? przekroczy?a nastaw?, uruchom alarm (mruganie)
            if(wartosc_potencjometru > nastawa_alarmowa) {
                stan_alarmu = ALARM_MRUGANIE;
                licznik_czasu = 0;       // Zresetuj licznik czasu
                licznik_mrugania = 0;    // Zresetuj licznik mrugania
                mruganie_stan = 0;      // Rozpocznij od zgaszonej diody
            }
            break;
            
        case ALARM_MRUGANIE:
            // Zwi?ksz licznik czasu
            licznik_czasu++;
            licznik_mrugania++;
            
            // Mruganie diod? co CZESTOTL_MRUGANIA cykli
            // *** TU MO?ESZ DOSTOSOWA? LOGIK? MRUGANIA ***
            if(licznik_mrugania >= CZESTOTL_MRUGANIA) {
                licznik_mrugania = 0;
                
                if(mruganie_stan == 0) {
                    LATA = 0x0001;  // Zapal pierwsz? diod?
                    mruganie_stan = 1;
                } else {
                    LATA = 0x0000;  // Zga? wszystkie diody
                    mruganie_stan = 0;
                }
            }
            
            // Po czasie okre?lonym przez CZAS_MRUGANIA przejd? do stanu ALARM_WSZYSTKIE
            // *** TU SPRAWDZANY JEST CA?KOWITY CZAS FAZY MRUGANIA ***
            if(licznik_czasu >= CZAS_MRUGANIA) {
                stan_alarmu = ALARM_WSZYSTKIE;
            }
            
            // Je?li warto?? spad?a poni?ej nastawy, wy??cz alarm
            if(wartosc_potencjometru < nastawa_alarmowa) {
                stan_alarmu = ALARM_OFF;
                LATA = 0x0000;  // Wy??cz wszystkie diody
            }
            break;
            
        case ALARM_WSZYSTKIE:
            // Zapal wszystkie diody
            LATA = 0x00FF;  // Zapal wszystkie 8 diód (je?li tyle jest dost?pnych)
            
            // Je?li warto?? spad?a poni?ej nastawy, wy??cz alarm
            if(wartosc_potencjometru < nastawa_alarmowa) {
                stan_alarmu = ALARM_OFF;
                LATA = 0x0000;  // Wy??cz wszystkie diody
            }
            break;
    }
}

// G?ówna funkcja programu
int main(void) {
    init();
    
    while(1) {
        alarm();
        
        delay(25);
    }
    
    return 0;
}