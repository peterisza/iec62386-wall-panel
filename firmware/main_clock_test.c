#include <msp430.h>

// Clock initialization: Set DCO to 8 MHz
void clock_init_8mhz_cursor(void)
{
    // Unlock CS registers (FRCTL0 password)
    FRCTL0 = FRCTLPW;  // Unlock FRCTL0
    
    // Configure DCO for ~8-10 MHz
    // CSCTL0: DCO tuning (clear initially for stability)
    CSCTL0 = 0;  // Clear DCO tuning bits
    
    // CSCTL1: DCORSEL and DCOFSEL
    // DCORSEL_3 = 0x0006 (8 MHz range)
    // DCOFSEL values: 0=1MHz, 1=1.5MHz, 2=2MHz, 3=3MHz, 4=4MHz, 5=6MHz, 6=8MHz, 7=12MHz
    // DCOFSEL=6 gives ~10.2 MHz (measured: 980 us for 10000 cycles)
    // But starts at 8.47 MHz (1.18 ms), then drifts to 10.2 MHz - DCO is unstable
    CSCTL1 = DCORSEL_3 | (6 << 3);  // DCORSEL=3 (8MHz range), DCOFSEL=6 (~10.2 MHz)
    
    // CSCTL2: Select clock sources
    // SELA__VLOCLK = 0x0200
    // SELS = DCO (bits 5-4 = 3, value = 0x0030)
    // SELM = DCO (bits 1-0 = 3, value = 0x0003)
    CSCTL2 = SELA__VLOCLK | (3 << 4) | 3;  // SELA=VLO, SELS=DCO, SELM=DCO
    
    // CSCTL3: Clock dividers (all = 1, no division)
    CSCTL3 = 0x0000;  // All dividers = 1
    
    // Wait for DCO to stabilize - use longer delay
    // At ~10 MHz, 10000 cycles = 1 ms, which should be enough
    // Note: DCO frequency may drift due to temperature/voltage changes
    __delay_cycles(10000);
}

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

static void clock_init_8mhz_chatgpt(void)
{
    //CSCTL0_H = CSKEY_H;                         // Unlock CS registers
    CSCTL0_H = 0xA5; 
    // FLL reference = REFO ~32.768 kHz, divider = 1
    CSCTL3 = SELREF__REFOCLK | FLLREFDIV__1;

    // DCO range select = 8 MHz range, modulation enabled (DISMOD=0)
    CSCTL1 = DCORSEL_3;                         // bits per CSCTL1 map :contentReference[oaicite:4]{index=4}

    // FLL: N = 243, D = 1  => DCO ≈ (243+1)*32768 = 7.995 MHz
    CSCTL2 = (0 << 12) | 243;                   // FLLD=0 (/1), FLLN=243 :contentReference[oaicite:5]{index=5}

    // Clock sources: MCLK+SMCLK = DCOCLKDIV, ACLK = REFO
    CSCTL4 = SELMS__DCOCLKDIV | SELA__REFOCLK;  // source select lives here :contentReference[oaicite:6]{index=6}

    // Clear fault flags and wait until FLL settles
    do {
        CSCTL7 &= ~(DCOFFG | FLLULIFG);         // clear DCO/FLL unlock flags
        SFRIFG1 &= ~OFIFG;                      // clear osc fault
    } while (SFRIFG1 & OFIFG);

    CSCTL0_H = 0;                               // Lock CS registers
}


int main(void)
{
   // Stop watchdog timer FIRST (critical!)
   WDTCTL = WDTPW | WDTHOLD;
   
   // Initialize clock to ~10 MHz
   clock_init_8mhz();
   
   // Wait longer for DCO to fully stabilize
   __delay_cycles(10000);
   
   // Unlock GPIO pins (important for FR series!)
   PM5CTL0 &= ~LOCKLPM5;
  
   // Small delay for GPIO stabilization
   __delay_cycles(1000);

   // Configure P2.0 as output
   P2DIR |= BIT0 | BIT1;
   P2OUT &= ~(BIT0 | BIT1);

   // Simple loop: toggle P2.0 every 50000 cycles (NEW CODE - different from old!)
   // At 8 MHz: 50000 cycles = 6250 µs, so period = 12500 µs (80 Hz)
   // At 1 MHz: 50000 cycles = 50000 µs, so period = 100000 µs (10 Hz)
   while(1) {
       P2OUT ^= BIT0;
       __delay_cycles(1000);  // Changed to 50000 to clearly see if new code runs
   }
}

