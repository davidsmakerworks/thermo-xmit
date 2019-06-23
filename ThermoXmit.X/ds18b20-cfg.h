/* 
 * Application-specific configuration values for DS18B20
 *
 * Requires definitions for:
 * DS18B20_TRIS - Bit value representing tri-state configuration of 1-Wire pin
 *                (1 = tri-state input, 0 = push/pull output)
 * DS18B20_DATA - Bit value representing push/pull output state of 1-Wire pin
 * 
 * Modify includes as necessary for processor architecture
 * 
 */

#ifndef DS18B20_CFG_H
#define	DS18B20_CFG_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <xc.h>

#define DS18B20_TRIS    TRISAbits.TRISA4
#define DS18B20_DATA    PORTAbits.RA4

#ifdef	__cplusplus
}
#endif

#endif	/* DS18B20_CFG_H */
