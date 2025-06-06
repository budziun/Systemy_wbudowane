// === BITY KONFIGURACYJNE MIKROKONTROLERA ===
// Dostosuj te ustawienia do swojej p?ytki i wymaga? projektu.
#pragma config POSCMOD = XT        // Tryb pierwotnego oscylatora (XT Oscillator Mode)
#pragma config OSCIOFNC = OFF      // Funkcja pinu OSC2/CLKO (CLKO Output Signal Active)
#pragma config FCKSM = CSDCMD      // Prze??czanie zegara i monitorowanie (Disabled)
#pragma config FNOSC = PRI         // Wybór ?ród?a oscylatora (Primary Oscillator: XT, HS, EC)
#pragma config IESO = ON           // Prze??czanie wewn?trznego/zewn?trznego oscylatora (Enabled)
 
#pragma config WDTPS = PS32768     // Postskaler Watchdog Timera
#pragma config FWPSA = PR128       // Preskaler WDT
#pragma config WINDIS = OFF        // Tryb okienkowy WDT (Non-Window Mode)
#pragma config FWDTEN = OFF        // W??czenie Watchdog Timera (Disabled)
 
#pragma config ICS = PGx2          // Piny dla programatora/debuggera (PGEC2/PGED2)
#pragma config JTAGEN = OFF        // Port JTAG (Disabled)
#pragma config GCP = OFF           // Ochrona kodu (Disabled)
#pragma config GWRP = OFF          // Ochrona zapisu (Disabled)
 
// === DEFINICJE I PLIKI NAG?ÓWKOWE ===
#define FCY 4000000UL               // Cz?stotliwo?? cyklu instrukcji (Fosc/2)
#define SYSTEM_PERIPHERAL_CLOCK FCY // Wymagane przez lcd.c
 
#include <xc.h>
#include <libpic30.h>  // Dla funkcji __delay_ms, __delay_us
#include <stdint.h>    // Dla typów uint16_t, uint8_t
#include <stdbool.h>   // Dla typu bool
#include <stdio.h>     // Dla funkcji sprintf
#include <string.h>    // Dla funkcji strlen
#include "lcd.h"       // Do??czona biblioteka LCD (zak?adamy, ?e jest w projekcie)
 
// === DEFINICJE PINÓW PRZYCISKÓW ===
#define BUTTON_PLAYER1_PORT   PORTDbits.RD6
#define BUTTON_PLAYER1_TRIS   TRISDbits.TRISD6
#define BUTTON_PLAYER1_CN_EN  CNEN1bits.CN15IE // RD6 to CN15 na PIC24FJ128GA010
 
#define BUTTON_PLAYER2_PORT   PORTDbits.RD13
#define BUTTON_PLAYER2_TRIS   TRISDbits.TRISD13
#define BUTTON_PLAYER2_CN_EN  CNEN2bits.CN19IE // RD13 to CN19 na PIC24FJ128GA010
 
// === DEFINICJE PINU POTENCJOMETRU ===
#define POT_ADC_CHANNEL       5 // AN5
#define POT_TRIS              TRISBbits.TRISB5 // Je?li AN5 jest na RB5
// Dla PIC24FJ128GA010, AN5 (RB5) jest kontrolowany przez AD1PCFG.
// Je?li u?ywasz innego PICa z rejestrami ANSEL, u?yj odpowiedniego bitu ANSEL.
// #define POT_ANSEL             ANSELBbits.ANSB5
 
// === STA?E I TYPY ===
const uint16_t GAME_TIMES_SECONDS_AVAILABLE[] = {60, 180, 300}; // 1 min, 3 min, 5 min
const uint8_t NUM_GAME_TIME_SETTINGS = sizeof(GAME_TIMES_SECONDS_AVAILABLE) / sizeof(GAME_TIMES_SECONDS_AVAILABLE[0]);
 
// Stany pracy zegara szachowego
typedef enum {
    STATE_SETUP_TIME,         // Ustawianie czasu gry
    STATE_PLAYER1_TIME_RUNNING, // Czas gracza 1 odlicza (tura gracza 2)
    STATE_PLAYER2_TIME_RUNNING, // Czas gracza 2 odlicza (tura gracza 1)
    STATE_GAME_OVER_P1_LOST,  // Gracz 1 przegra? na czas
    STATE_GAME_OVER_P2_LOST   // Gracz 2 przegra? na czas
} ChessClockState_t;
 
// === ZMIENNE GLOBALNE ===
volatile ChessClockState_t current_clock_state = STATE_SETUP_TIME;
volatile uint16_t player1_remaining_seconds = 0;
volatile uint16_t player2_remaining_seconds = 0;
volatile uint8_t selected_initial_time_index = 0; // Indeks wybranego czasu pocz?tkowego
 
volatile bool display_needs_update = true;    // Flaga sygnalizuj?ca potrzeb? od?wie?enia LCD
volatile bool button_action_pending = false; // Flaga sygnalizuj?ca wci?ni?cie przycisku (obs?ugiwane w p?tli g?ównej)
 
// Bufory dla wy?wietlacza LCD (16 znaków + znak null)
char lcd_line1_buffer[17];
char lcd_line2_buffer[17];
 
// === PROTOTYPY FUNKCJI ===
void setup_hardware(void);
void update_lcd_content(void);
void process_button_actions(void);
void read_potentiometer_and_update_setting(void);
void start_selected_game(uint8_t starting_player_whose_time_runs); // 1 dla P1, 2 dla P2
 
// === G?ÓWNA FUNKCJA PROGRAMU ===
int main(void) {
    setup_hardware();
 
    while (1) {
        if (current_clock_state == STATE_SETUP_TIME) {
            read_potentiometer_and_update_setting();
        }
 
        if (button_action_pending) {
            process_button_actions();
            button_action_pending = false;
        }
 
        if (display_needs_update) {
            update_lcd_content();
            display_needs_update = false;
        }
        // Idle(); // Mo?na doda? tryb u?pienia, aby oszcz?dza? energi?
    }
    return 0;
}
 
// === FUNKCJA INICJALIZACJI SPRZ?TU ===
void setup_hardware(void) {
    // Konfiguracja portów I/O dla przycisków
    BUTTON_PLAYER1_TRIS = 1;  // Ustaw jako wej?cie
    BUTTON_PLAYER2_TRIS = 1;  // Ustaw jako wej?cie
 
    // Konfiguracja pinu potencjometru jako wej?cie analogowe
    // Dla PIC24FJ128GA010, AN5 jest na RB5.
    TRISBbits.TRISB5 = 1;      // Ustaw RB5 (AN5) jako wej?cie
    AD1PCFGbits.PCFG5 = 0;     // Skonfiguruj AN5 jako wej?cie analogowe (bit PCFG5=0)
                               // Upewnij si?, ?e inne bity AD1PCFG s? poprawnie ustawione,
                               // je?li u?ywasz innych pinów analogowych.
                               // AD1PCFG = 0xFFFF; // Wszystkie jako cyfrowe
                               // AD1PCFGbits.PCFG5 = 0; // Tylko AN5 jako analogowe
 
    // Inicjalizacja ADC (prosta konfiguracja)
    AD1CON1 = 0x00E0; // SSRC=111 (auto-convert), ASAM=0 (manual sample start), FORM=00 (Integer)
    AD1CON2 = 0x0000; // VCFG=AVdd/AVss, bez skanowania
    AD1CON3 = 0x1F02; // SAMC=31 TAD (auto sample time), ADCS=2 (TAD = TCY*3 -> ~2.25us TAD @ 4MHz FCY)
                      // Ca?kowity czas próbkowania + konwersji b?dzie zarz?dzany przez SSRC=7 i SAMC
    AD1CON1bits.ADON = 1; // W??cz modu? ADC
 
    // Inicjalizacja wy?wietlacza LCD
    if (!LCD_Initialize()) {
        // Obs?uga b??du inicjalizacji LCD
        while(1); // Zawie? program
    }
    LCD_CursorEnable(false); // Wy??cz kursor
 
    // Konfiguracja Timera1 do generowania przerwania co 1 sekund?
    T1CONbits.TON = 0;      // Wy??cz Timer1 na czas konfiguracji
    T1CONbits.TCS = 0;      // Zegar wewn?trzny (FOSC/2)
    T1CONbits.TGATE = 0;    // Gated time accumulation wy??czone
    T1CONbits.TCKPS = 0b11; // Preskaler 1:256
    TMR1 = 0;               // Wyczy?? licznik Timera1
    PR1 = (FCY / 256 / 1) - 1; // Warto?? okresu dla 1 sekundy (15624 dla FCY=4MHz)
   
    IPC0bits.T1IP = 4;      // Ustaw priorytet przerwania Timer1 (np. 4)
    IFS0bits.T1IF = 0;      // Wyczy?? flag? przerwania Timer1
    IEC0bits.T1IE = 1;      // W??cz przerwania od Timer1
    // T1CONbits.TON = 0;   // Timer zostanie w??czony po starcie gry
 
    // Konfiguracja przerwa? od zmiany stanu (Change Notification) dla przycisków
    BUTTON_PLAYER1_CN_EN = 1;
    BUTTON_PLAYER2_CN_EN = 1;
 
    // Opcjonalnie: W??czenie wewn?trznych rezystorów podci?gaj?cych (pull-up)
    // Dla PIC24FJ128GA010:
    // CNPU1bits.CN15PUE = 1; // Dla RD6
    // CNPU2bits.CN19PUE = 1; // Dla RD13
 
    IFS1bits.CNIF = 0;      // Wyczy?? flag? przerwania CN
    IPC4bits.CNIP = 5;      // Ustaw priorytet przerwania CN (np. 5, wy?szy ni? Timer1)
    IEC1bits.CNIE = 1;      // W??cz globalnie przerwania CN
 
    // Stan pocz?tkowy
    read_potentiometer_and_update_setting(); // Odczytaj czas pocz?tkowy
    player1_remaining_seconds = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index];
    player2_remaining_seconds = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index];
    current_clock_state = STATE_SETUP_TIME;
    display_needs_update = true;
}
 
// === FUNKCJA ODCZYTU POTENCJOMETRU I AKTUALIZACJI USTAWIE? CZASU ===
void read_potentiometer_and_update_setting(void) {
    uint16_t adc_raw_value;
    uint8_t previously_selected_index = selected_initial_time_index;
 
    AD1CHS = POT_ADC_CHANNEL;   // Wybierz kana? AN5 dla ADC
    AD1CON1bits.SAMP = 1;       // Rozpocznij próbkowanie
    // Czas próbkowania jest okre?lony przez SAMC (31 TAD) z SSRC=7 (auto-convert)
    // Konwersja rozpocznie si? automatycznie po zako?czeniu próbkowania.
    __delay_us(50); // Daj troch? czasu na ustabilizowanie si? próbki przed rozpocz?ciem konwersji
                    // Cho? przy SSRC=7 SAMC powinno wystarczy?, to dla pewno?ci.
    AD1CON1bits.SAMP = 0;       // R?czne zako?czenie próbkowania (mo?e by? potrzebne przy SSRC=7 i ASAM=0,
                                // je?li SAMC nie jest wystarczaj?co d?ugi lub chcemy precyzyjnej kontroli)
    while (!AD1CON1bits.DONE);  // Czekaj na zako?czenie konwersji
    adc_raw_value = ADC1BUF0;   // Odczytaj warto?? z ADC
 
    // Mapowanie warto?ci ADC (0-1023) na indeks czasu w tablicy
    // Dzielimy zakres ADC na NUM_GAME_TIME_SETTINGS cz??ci
    uint16_t step = 1024 / NUM_GAME_TIME_SETTINGS;
    if (adc_raw_value < step) { // 0 - (1024/3 - 1)
        selected_initial_time_index = 0; // 1 min
    } else if (adc_raw_value < (step * 2)) { // (1024/3) - (2*1024/3 - 1)
        selected_initial_time_index = 1; // 3 min
    } else { // (2*1024/3) - 1023
        selected_initial_time_index = 2; // 5 min
    }
 
    if (previously_selected_index != selected_initial_time_index) {
        // Je?li ustawienie czasu si? zmieni?o, zaktualizuj czasy dla graczy
        player1_remaining_seconds = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index];
        player2_remaining_seconds = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index];
        display_needs_update = true;
    }
}
 
// === FUNKCJA AKTUALIZACJI WY?WIETLACZA LCD ===
void update_lcd_content(void) {
    uint16_t p1_min, p1_sec, p2_min, p2_sec;
 
    LCD_ClearScreen(); // Czy?ci ekran i ustawia kursor na 0,0
 
    switch (current_clock_state) {
        case STATE_SETUP_TIME:
            p1_min = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index] / 60;
            p1_sec = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index] % 60;
            sprintf(lcd_line1_buffer, "Ustaw Czas:");
            // Dope?nij spacjami do 16 znaków, je?li potrzeba
            for(int i = strlen(lcd_line1_buffer); i < 16; i++) lcd_line1_buffer[i] = ' ';
            lcd_line1_buffer[16] = '\0';
            LCD_PutString(lcd_line1_buffer, 16);
 
            LCD_PutChar('\n'); // Przejd? do drugiej linii (obs?ugiwane przez lcd.c)
 
            sprintf(lcd_line2_buffer, "   %02u:%02u Min   ", p1_min, p1_sec);
            for(int i = strlen(lcd_line2_buffer); i < 16; i++) lcd_line2_buffer[i] = ' ';
            lcd_line2_buffer[16] = '\0';
            LCD_PutString(lcd_line2_buffer, 16);
            break;
 
        case STATE_PLAYER1_TIME_RUNNING:
        case STATE_PLAYER2_TIME_RUNNING:
            p1_min = player1_remaining_seconds / 60;
            p1_sec = player1_remaining_seconds % 60;
            p2_min = player2_remaining_seconds / 60;
            p2_sec = player2_remaining_seconds % 60;
 
            sprintf(lcd_line1_buffer, "G1:%02u:%02u%s", p1_min, p1_sec, (current_clock_state == STATE_PLAYER1_TIME_RUNNING ? " *" : "  "));
            for(int i = strlen(lcd_line1_buffer); i < 16; i++) lcd_line1_buffer[i] = ' ';
            lcd_line1_buffer[16] = '\0';
            LCD_PutString(lcd_line1_buffer, 16);
 
            LCD_PutChar('\n');
 
            sprintf(lcd_line2_buffer, "G2:%02u:%02u%s", p2_min, p2_sec, (current_clock_state == STATE_PLAYER2_TIME_RUNNING ? " *" : "  "));
            for(int i = strlen(lcd_line2_buffer); i < 16; i++) lcd_line2_buffer[i] = ' ';
            lcd_line2_buffer[16] = '\0';
            LCD_PutString(lcd_line2_buffer, 16);
            break;
 
        case STATE_GAME_OVER_P1_LOST: // Gracz 1 przegra?
            sprintf(lcd_line1_buffer, "GRACZ 2 WYGRAL!");
            for(int i = strlen(lcd_line1_buffer); i < 16; i++) lcd_line1_buffer[i] = ' ';
            lcd_line1_buffer[16] = '\0';
            LCD_PutString(lcd_line1_buffer, 16);
           
            LCD_PutChar('\n');
           
            sprintf(lcd_line2_buffer, "G1: KoniecCzasu");
            for(int i = strlen(lcd_line2_buffer); i < 16; i++) lcd_line2_buffer[i] = ' ';
            lcd_line2_buffer[16] = '\0';
            LCD_PutString(lcd_line2_buffer, 16);
            break;
 
        case STATE_GAME_OVER_P2_LOST: // Gracz 2 przegra?
            sprintf(lcd_line1_buffer, "GRACZ 1 WYGRAL!");
            for(int i = strlen(lcd_line1_buffer); i < 16; i++) lcd_line1_buffer[i] = ' ';
            lcd_line1_buffer[16] = '\0';
            LCD_PutString(lcd_line1_buffer, 16);
 
            LCD_PutChar('\n');
 
            sprintf(lcd_line2_buffer, "G2: KoniecCzasu");
            for(int i = strlen(lcd_line2_buffer); i < 16; i++) lcd_line2_buffer[i] = ' ';
            lcd_line2_buffer[16] = '\0';
            LCD_PutString(lcd_line2_buffer, 16);
            break;
    }
}
 
// === FUNKCJA OBS?UGI PRZYCISKÓW ===
void process_button_actions(void) {
    // Prosty debouncing przez opó?nienie
    // Lepszym podej?ciem by?by debouncing programowy oparty na timerze
    // lub sprawdzanie stanu przycisku wielokrotnie w krótkich odst?pach.
    __delay_ms(50); // Czekaj, a? drgania styków ustan?
 
    bool p1_pressed_flag = false;
    bool p2_pressed_flag = false;
 
    if (!BUTTON_PLAYER1_PORT) {
        p1_pressed_flag = true;
        while(!BUTTON_PLAYER1_PORT); // Czekaj na zwolnienie przycisku
        __delay_ms(20); // Dodatkowy debounce po zwolnieniu
    }
    if (!BUTTON_PLAYER2_PORT) {
        p2_pressed_flag = true;
        while(!BUTTON_PLAYER2_PORT); // Czekaj na zwolnienie przycisku
        __delay_ms(20); // Dodatkowy debounce po zwolnieniu
    }
 
    // Logika gry w zale?no?ci od stanu i wci?ni?tego przycisku
    if (current_clock_state == STATE_SETUP_TIME) {
        if (p1_pressed_flag || p2_pressed_flag) { // Dowolny przycisk startuje gr?
            // Ustaw czasy pocz?tkowe na podstawie wybranego z potencjometru
            player1_remaining_seconds = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index];
            player2_remaining_seconds = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index];
           
            // Zak?adamy, ?e gracz, który nie nacisn?? przycisku startowego, wykonuje pierwszy ruch,
            // wi?c czas gracza, który nacisn?? przycisk, zaczyna odlicza?.
            // Lub, standardowo, P1 naciska, czas P2 rusza.
            if (p1_pressed_flag) {
                 current_clock_state = STATE_PLAYER2_TIME_RUNNING; // Czas P2 odlicza
            } else { // p2_pressed_flag
                 current_clock_state = STATE_PLAYER1_TIME_RUNNING; // Czas P1 odlicza
            }
            T1CONbits.TON = 1; // W??cz Timer1
            display_needs_update = true;
        }
    } else if (current_clock_state == STATE_PLAYER1_TIME_RUNNING) {
        if (p2_pressed_flag) { // Gracz 2 nacisn?? (nie jego tura) - mo?na zignorowa?
            // Nic nie rób lub ewentualnie jaka? logika kary
        } else if (p1_pressed_flag) { // Gracz 1 nacisn??, ko?czy swoj? tur?
            current_clock_state = STATE_PLAYER2_TIME_RUNNING;
            display_needs_update = true;
        }
    } else if (current_clock_state == STATE_PLAYER2_TIME_RUNNING) {
        if (p1_pressed_flag) { // Gracz 1 nacisn?? (nie jego tura)
            // Nic nie rób
        } else if (p2_pressed_flag) { // Gracz 2 nacisn??, ko?czy swoj? tur?
            current_clock_state = STATE_PLAYER1_TIME_RUNNING;
            display_needs_update = true;
        }
    } else if (current_clock_state == STATE_GAME_OVER_P1_LOST || current_clock_state == STATE_GAME_OVER_P2_LOST) {
        if (p1_pressed_flag || p2_pressed_flag) { // Dowolny przycisk resetuje do ustawie?
            current_clock_state = STATE_SETUP_TIME;
            T1CONbits.TON = 0; // Zatrzymaj timer
            read_potentiometer_and_update_setting(); // Odczytaj nowe ustawienie czasu
            player1_remaining_seconds = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index];
            player2_remaining_seconds = GAME_TIMES_SECONDS_AVAILABLE[selected_initial_time_index];
            display_needs_update = true;
        }
    }
}
 
// === PROCEDURA OBS?UGI PRZERWANIA TIMERA1 (CO 1 SEKUND?) ===
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void) {
    IFS0bits.T1IF = 0; // Wyczy?? flag? przerwania Timer1
 
    if (current_clock_state == STATE_PLAYER1_TIME_RUNNING) {
        if (player1_remaining_seconds > 0) {
            player1_remaining_seconds--;
        }
        if (player1_remaining_seconds == 0) { // Sprawd? PO dekrementacji
            current_clock_state = STATE_GAME_OVER_P1_LOST;
            T1CONbits.TON = 0; // Zatrzymaj timer
        }
        display_needs_update = true;
    } else if (current_clock_state == STATE_PLAYER2_TIME_RUNNING) {
        if (player2_remaining_seconds > 0) {
            player2_remaining_seconds--;
        }
        if (player2_remaining_seconds == 0) { // Sprawd? PO dekrementacji
            current_clock_state = STATE_GAME_OVER_P2_LOST;
            T1CONbits.TON = 0; // Zatrzymaj timer
        }
        display_needs_update = true;
    }
}
 
// === PROCEDURA OBS?UGI PRZERWANIA OD ZMIANY STANU (CN) DLA PRZYCISKÓW ===
void __attribute__((interrupt, auto_psv)) _CNInterrupt(void) {
    // Sprawd?, czy przerwanie faktycznie pochodzi od wci?ni?cia przycisku (stan niski),
    // a nie od zwolnienia, je?li piny CN s? wra?liwe na obie zmiany.
    // Dla prostoty, zak?adamy, ?e przerwanie wyst?puje przy wci?ni?ciu.
    // Debouncing i obs?uga logiki przycisku zostan? wykonane w p?tli g?ównej.
   
    // Minimalny kod w ISR - tylko ustawienie flagi.
    // Sprawdzenie, który przycisk spowodowa? przerwanie, mo?e by? wykonane
    // w process_button_actions() przez odczytanie portów.
    // To pozwala unikn?? problemów z wieloma flagami, je?li oba przyciski
    // zosta?yby naci?ni?te niemal jednocze?nie.
   
    if(!BUTTON_PLAYER1_PORT || !BUTTON_PLAYER2_PORT){ // Sprawd? czy którykolwiek jest wci?ni?ty
         button_action_pending = true;
    }
   
    IFS1bits.CNIF = 0; // Zawsze czy?? flag? przerwania CN na ko?cu
}