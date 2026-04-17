#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "sl_gpio.h"

// $[CMU]
// [CMU]$

// $[LFXO]
// [LFXO]$

// $[PRS.ASYNCH0]
// [PRS.ASYNCH0]$

// $[PRS.ASYNCH1]
// [PRS.ASYNCH1]$

// $[PRS.ASYNCH2]
// [PRS.ASYNCH2]$

// $[PRS.ASYNCH3]
// [PRS.ASYNCH3]$

// $[PRS.ASYNCH4]
// [PRS.ASYNCH4]$

// $[PRS.ASYNCH5]
// [PRS.ASYNCH5]$

// $[PRS.ASYNCH6]
// [PRS.ASYNCH6]$

// $[PRS.ASYNCH7]
// [PRS.ASYNCH7]$

// $[PRS.ASYNCH8]
// [PRS.ASYNCH8]$

// $[PRS.ASYNCH9]
// [PRS.ASYNCH9]$

// $[PRS.ASYNCH10]
// [PRS.ASYNCH10]$

// $[PRS.ASYNCH11]
// [PRS.ASYNCH11]$

// $[PRS.SYNCH0]
// [PRS.SYNCH0]$

// $[PRS.SYNCH1]
// [PRS.SYNCH1]$

// $[PRS.SYNCH2]
// [PRS.SYNCH2]$

// $[PRS.SYNCH3]
// [PRS.SYNCH3]$

// $[GPIO]
// [GPIO]$

// $[TIMER0]
// [TIMER0]$

// $[TIMER1]
// [TIMER1]$

// $[TIMER2]
// [TIMER2]$

// $[TIMER3]
// [TIMER3]$

// $[TIMER4]
// [TIMER4]$

// $[USART0]
// [USART0]$

// $[I2C1]
// [I2C1]$

// $[EUSART1]
// [EUSART1]$

// $[EUSART2]
// [EUSART2]$

// $[LCD]
// [LCD]$

// $[KEYSCAN]
// [KEYSCAN]$

// $[LETIMER0]
// [LETIMER0]$

// $[IADC0]
// [IADC0]$

// $[ACMP0]
// [ACMP0]$

// $[ACMP1]
// [ACMP1]$

// $[VDAC0]
// [VDAC0]$

// $[PCNT0]
// [PCNT0]$

// $[LESENSE]
// [LESENSE]$

// $[I2C0]
// [I2C0]$

// $[EUSART0]
// [EUSART0]$

// $[PTI]
// [PTI]$

// $[MODEM]
// [MODEM]$

// $[CUSTOM_PIN_NAME]
#ifndef _PORT                                   
#define _PORT                                    SL_GPIO_PORT_A
#endif
#ifndef _PIN                                    
#define _PIN                                     0
#endif

// <<< sl:start pin_tool >>>
// <i2c signal=SCL,SDA> SL_I2C_BUS
// I2C1 on PD02/PD03
#define SL_I2C_BUS_PERIPHERAL                    I2C1
#define SL_I2C_BUS_PERIPHERAL_NO                 1
#define SL_I2C_BUS_CLOCK                        cmuClock_I2C1

// I2C1 SCL on PD02
#define SL_I2C_BUS_SCL_PORT                      SL_GPIO_PORT_D
#define SL_I2C_BUS_SCL_PIN                       2

// I2C1 SDA on PD03
#define SL_I2C_BUS_SDA_PORT                      SL_GPIO_PORT_D
#define SL_I2C_BUS_SDA_PIN                       3

// <gpio> DEBUG_LED_D103
// D103 on PA07
#define DEBUG_LED_D103_PORT                      SL_GPIO_PORT_A
#define DEBUG_LED_D103_PIN                       7

// <gpio> DEBUG_LED_D102
// D102 on PA08
#define DEBUG_LED_D102_PORT                      SL_GPIO_PORT_A
#define DEBUG_LED_D102_PIN                       8

// <gpio> DEBUG_LED_D101
// D101 on PA09
#define DEBUG_LED_D101_PORT                      SL_GPIO_PORT_A
#define DEBUG_LED_D101_PIN                       9
// <<< sl:end pin_tool >>>































// [CUSTOM_PIN_NAME]$


#endif // PIN_CONFIG_H
