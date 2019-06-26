/*
 * Remote Temperature Sensor Transmitter Module
 * Copyright (C) 2019 David Rice
 * 
 * Processor: PIC16F18325
 *
 * The following macros must be defined on the XC8 command line or in project properties:
 * _XTAL_FREQ - CPU speed in Hz (Fosc) - must be 32000000 for this driver
 * RF_CHANNEL - the channel to use for the RF module (example: 0x10U)
 * 
 * Drivers used:
 * DS18B20
 * NRF24L01P
 * 
 * Peripheral usage:
 * SPI2 - Communication with RF module
 * Timer0 - Used by DS18B20 driver to track timing of 1-Wire protocol
 * 
 * Pin assignments:
 * RA4 - Temperature sensor 1-Wire data
 * RC0 - RF module CE
 * RC1 - RF module CSN
 * RC2 - RF module SCK
 * RC3 - RF module MOSI
 * RC4 - RF module MISO
 * RC5 - RF module IRQ
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// PIC16F18325 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1
#pragma config FEXTOSC = OFF    // FEXTOSC External Oscillator mode Selection bits (Oscillator not enabled)
#pragma config RSTOSC = HFINT32 // Power-up default value for COSC bits (HFINTOSC with 2x PLL (32MHz))
#pragma config CLKOUTEN = OFF   // Clock Out Enable bit (CLKOUT function is disabled; I/O or oscillator function on OSC2)
#pragma config CSWEN = ON       // Clock Switch Enable bit (Writing to NOSC and NDIV is allowed)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)

// CONFIG2
#pragma config MCLRE = ON       // Master Clear Enable bit (MCLR/VPP pin function is MCLR; Weak pull-up enabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config WDTE = OFF       // Watchdog Timer Enable bits (WDT disabled; SWDTEN is ignored)
#pragma config LPBOREN = OFF    // Low-power BOR enable bit (ULPBOR disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable bits (Brown-out Reset enabled, SBOREN bit ignored)
#pragma config BORV = LOW       // Brown-out Reset Voltage selection bit (Brown-out voltage (Vbor) set to 2.45V)
#pragma config PPS1WAY = ON     // PPSLOCK bit One-Way Set Enable bit (The PPSLOCK bit can be cleared and set only once; PPS registers remain locked after one clear/set cycle)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable bit (Stack Overflow or Underflow will cause a Reset)
#pragma config DEBUG = OFF      // Debugger enable bit (Background debugger disabled)

// CONFIG3
#pragma config WRT = OFF        // User NVM self-write protection bits (Write protection off)
#pragma config LVP = ON         // Low Voltage Programming Enable bit (Low Voltage programming enabled. MCLR/VPP pin function is MCLR. MCLRE configuration bit is ignored.)

// CONFIG4
#pragma config CP = OFF         // User NVM Program Memory Code Protection bit (User NVM code protection disabled)
#pragma config CPD = OFF        // Data NVM Memory Code Protection bit (Data NVM code protection disabled)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "nRF24L01P.h"
#include "nRF24L01P-cfg.h"

#include "ds18b20.h"
#include "ds18b20-cfg.h"

#ifndef RF_CHANNEL
#error RF_CHANNEL must be defined
#endif

#if _XTAL_FREQ != 32000000
#error _XTAL_FREQ must be defined as 32000000 (this project requires Fosc = 32 MHz)
#endif

#define ADDR_LEN        5
#define PAYLOAD_WIDTH   2

const uint8_t display_addr[ADDR_LEN] = { 'T', 'E', 'M', 'P', 0xA5 };

/* 
 * Standard port initialization
 */
void init_ports(void) {
    /* Disable all analog features */
    ANSELA = 0x00;
    ANSELC = 0x00;
    
    /* Pull all outputs low except RC1 (RF_CSN) */
    LATA = 0x00;
    LATC = _LATC_LATC1_MASK;
    
    /* Set all ports to output except RA4 (temperature sensor), RC4 (SDI2), and RC5 (RF_IRQ) */
    TRISA = _TRISA_TRISA4_MASK;
    TRISC = _TRISC_TRISC4_MASK |
            _TRISC_TRISC5_MASK;
    
    /* Set TTL on RC4 (SDI2) and RC5 (RF_IRQ) due to 3.3V output from RF module */
    INLVLCbits.INLVLC4 = 0;
    INLVLCbits.INLVLC5 = 0;
}

/* Initialize SPI peripheral that will drive the RF module */
void init_mssp(void) {
    /* MSSP2 (SPI master) used to drive RF module */
    
    /* Set MSSP2 to SPI Master mode using baud rate generator */
    SSP2CON1bits.SSPM = 0b1010;
    
    /* 1 MHz at Fosc = 32 MHz */
    SSP2ADD = 7;
    
    /* Transmit data on active-to-idle transition */
    SSP2STATbits.CKE = 1;
    
    /* Enable MSSP2 */
    SSP2CON1bits.SSPEN = 1;
}

/* Initialize PPS module to route signals to pins */
void init_pps(void) {
    bool state;
    
    /* Preserve global interrupt state and disable interrupts */
    state = INTCONbits.GIE;
    INTCONbits.GIE = 0;
    
    /* Unlock PPS */
    PPSLOCK = 0x55;
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 0;
    
    /* SCK2 on RC2 */
    RC2PPS = 0b11010;
    SSP2CLKPPS = 0b10010;
    
    /* SDI2 on RC4 */
    SSP2DATPPS = 0b10100;
    
    /* SDO2 on RC3 */
    RC3PPS = 0b11011;
    
    /* Lock PPS */
    PPSLOCK = 0x55;
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 1;
    
    /* Restore global interrupt state */
    INTCONbits.GIE = state;
}

/* Initialize NRF24L01+ module in transmit mode */
void init_rf(void) {
    /* Allow for maximum possible RF module startup time */
    __delay_ms(100);
    
    /* Set 500 uSec retry interval and 4 maximum retries */
    nrf24_write_register(NRF24_SETUP_RETR, NRF24_ARD_500 | NRF24_ARC_4);
    
    /* Set RF power to 0 dBm and data rate to 1 Mbit/Sec */
    nrf24_write_register(NRF24_RF_SETUP, NRF24_RF_PWR_0DBM);
    
    /* Set 5-byte address width */
    nrf24_write_register(NRF24_SETUP_AW, NRF24_AW_5);
    
    /* Set initial RF channel */
    nrf24_write_register(NRF24_RF_CH, RF_CHANNEL);
    
    /* Mask RX_DR interrupt on RF module, enable CRC, power up RF module in transmit-standby mode */
    nrf24_write_register(NRF24_CONFIG, NRF24_MASK_RX_DR | NRF24_EN_CRC | NRF24_PWR_UP);
    
    /* Set TX and RX addresses */
    nrf24_set_rx_address(NRF24_RX_ADDR_P0, display_addr, ADDR_LEN);
    nrf24_set_tx_address(display_addr, ADDR_LEN);
    
    /* Clear any pending RF module interrupts */
    nrf24_write_register(NRF24_STATUS, NRF24_TX_DS | NRF24_MAX_RT | NRF24_RX_DR);
}

/* Initialize DS18B20 temeperature sensor */
void init_temp_sensor(void) {
    ds18b20_init_timer();
}

/* Transfer one byte on MSSP2 (SPI master) */
uint8_t transfer_spi(uint8_t data) {
    SSP2BUF = data;
    
    while (!SSP2STATbits.BF);
    
    data = SSP2BUF;
    
    return data;
}

/* 
 * Send a single packet via the RF module
 *
 * Returns true if packet was acknowledged by receiver, false otherwise
 */
bool send_packet(uint8_t *buf, uint8_t len)
{
    uint8_t status;
    
    /* Flush buffer as a brute-force way of handling unexpected initial conditions */
    nrf24_flush_tx();
    
    /* Write paylod to buffer - note that this does not actually transmit any data */
    nrf24_write_payload(buf, len);

    /* Strobe RF CE line to send one packet of data */
    NRF24_CE_ACTIVE();
    __delay_us(15);
    NRF24_CE_IDLE();

    /* 
     * Wait for IRQ line to go low, indicating that packet has been procesed
     * 
     * TODO: Add a timeout to prevent getting stuck here forever inthe event of
     * unexpected conditions */
    while (NRF24_IRQ);
    
    /* Get status of transmit attempt */
    status = nrf24_read_register(NRF24_STATUS);
    
    /* Clear any pending TX-related interrupts */
    nrf24_write_register(NRF24_STATUS, NRF24_TX_DS | NRF24_MAX_RT);
    
    /* If the NRF24_MAX_RT (max retries) bit is set, that means the packet was not received */
    if (status & NRF24_MAX_RT) {
        return false;
    } else {
        return true;
    }
}

void main(void) {
    int16_t temperature = 0;
    int16_t last_temperature = DS18B20_INVALID_TEMPERATURE;
    
    /* Initialize peripherals */
    init_ports();
    init_pps();
    init_mssp();
    init_rf();
    init_temp_sensor();
    
    /* 11-bit resolution means a resolution of 0.25 deg C */
    ds18b20_set_resolution(DS18B20_RES_11BIT);
    
    while(1) {
        if (ds18b20_start_conversion(true)) {
            temperature = ds18b20_get_temperature();
            
            if ((temperature != last_temperature) && (temperature != DS18B20_INVALID_TEMPERATURE)) {
                send_packet(&temperature, PAYLOAD_WIDTH);
                
                last_temperature = temperature;
            }
        }
    }
}
