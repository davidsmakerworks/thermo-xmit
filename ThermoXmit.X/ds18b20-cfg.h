/* 
 * Application-specific configuration values for DS18B20
 *
 * Requires definitions for:
 * DS18B20_PULL_BUS_LOW() - Macro to pull 1-Wire bus low
 * DS18B20_RELEASE_BUS() - Macro to release 1-Wire bus
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
    
#define DS18B20_PULL_BUS_LOW()  TRISAbits.TRISA4 = 0
#define DS18B20_RELEASE_BUS()   TRISAbits.TRISA4 = 1

#define DS18B20_DATA    PORTAbits.RA4

#ifdef	__cplusplus
}
#endif

#endif	/* DS18B20_CFG_H */
