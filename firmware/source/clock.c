#include <msp430.h>
#include "clock.h"

// Clock initialization: Set DCO to 8 MHz using FLL
void clock_init_8mhz(void)
{
    // 1. FRAM Wait State beállítása
    // Ha 8 MHz fölé mennénk (akár csak a beállás alatt), kell az 1 wait state.
    // 8 MHz-hez elvileg elég a 0, de a biztonság kedvéért az FLL beállás idejére ajánlott.
    FRCTL0 = FRCTLPW | NWAITS_1;

    // 2. FLL konfigurálása
    __bis_SR_register(SCG0);    // FLL hurok kikapcsolása a beállítás idejére

    // DCO beállítása 8 MHz tartományra
    // DCORSEL_3: 8 MHz közeli tartomány
    // DCOFSEL_3: 8 MHz specifikus beállítás
    //CSCTL1 = DCORSEL_3 | DCOFSEL_3;
    CSCTL1 = DCORSEL_3 | BIT1 | BIT2;

    // Referencia órajel kiválasztása: 32.768 kHz REFO
    CSCTL3 = SELREF__REFOCLK;   // FLL referencia = REFO

    // Szorzó beállítása (Multiplier)
    // Cél: 8 MHz. Ref: 32.768 kHz.
    // Képlet: (FLLN + 1) * 32768 = Cél frekvencia
    // 8000000 / 32768 = 244.14 -> 244
    // FLLN = 244 - 1 = 243
    CSCTL2 = FLLD_0 | 243;      // FLLD_0 = /1 osztó, FLLN = 243

    __bic_SR_register(SCG0);    // FLL visszakapcsolása

    // Várakozás, amíg az FLL stabilizálódik
    // A hardveres FLL-nek idő kell, amíg "ráhúzza" a DCO-t a helyes frekvenciára.
    do
    {
        CSCTL7 &= ~(XT1OFFG | DCOFFG); // Hibajelzők törlése
        SFRIFG1 &= ~OFIFG;             // Globalis oszcillátor hiba törlése
        __delay_cycles(1000);          // Kis várakozás
    } while (SFRIFG1 & OFIFG);         // Ha még mindig van hiba, ismétel
    
    // 3. Órajel források kiosztása
    // MCLK = DCO (8MHz), SMCLK = DCO (8MHz), ACLK = VLO vagy REFO
    // Megjegyzés: A CSCTL4 (FR2xxx) vagy CSCTL5 szabályozza a kimeneteket
    CSCTL4 = SELA__VLOCLK | SELMS__DCOCLKDIV; // ACLK=VLO, MCLK/SMCLK=DCO

    // Opcionális: Ha stabil 8MHz-en vagyunk, vissza lehet venni a wait state-et 0-ra
    // a kicsit gyorsabb működésért, de 1-gyel is tökéletesen működik.
    FRCTL0 = FRCTLPW | NWAITS_0; 
}
