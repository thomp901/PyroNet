/*
 *  ======== user_rf_switch_cfg.c ========
 *
 *  BDE-MB1352P71 RF switch configuration.
 *
 *  Why this file exists:
 *  - The BDE module uses a different Sub-1 GHz RF switch truth table than the
 *    TI LP_CC1352P7_1 LaunchPad examples.
 *  - The BDE module routes DIO29/DIO30 internally to the RF switch.
 *  - RF switch behavior must be controlled by the RF driver callback, not by
 *    ordinary application GPIO toggling.
 *
 *  BDE truth table:
 *      Off : DIO29 = 0, DIO30 = 0
 *      TX  : DIO29 = 1, DIO30 = 0
 *      RX  : DIO29 = 0, DIO30 = 1
 *
 *  This file deliberately uses the BDE module DIOs directly instead of
 *  LaunchPad antenna switch aliases.
 */

#include <stdbool.h>
#include <stdint.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/rf_ble_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_prop_cmd.h)

#include <ti/drivers/GPIO.h>
#include <ti/drivers/rf/RF.h>

#define BDE_RF_SWITCH_TX_PIN    29
#define BDE_RF_SWITCH_RX_PIN    30

static void bdeRfSwitchOff(void)
{
    GPIO_setConfigAndMux(BDE_RF_SWITCH_TX_PIN,
                         GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
                         IOC_PORT_GPIO);
    GPIO_setConfigAndMux(BDE_RF_SWITCH_RX_PIN,
                         GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
                         IOC_PORT_GPIO);
    GPIO_write(BDE_RF_SWITCH_TX_PIN, 0);
    GPIO_write(BDE_RF_SWITCH_RX_PIN, 0);
}

static bool bdeRadioSetupIsSub1GHz(const RF_RadioSetup *setupCommand)
{
    uint8_t loDivider = 0;

    if (setupCommand == NULL) {
        return false;
    }

    switch (setupCommand->commandId.commandNo) {
        case CMD_RADIO_SETUP:
            loDivider = RF_LODIVIDER_MASK & setupCommand->common.loDivider;
            break;

        case CMD_BLE5_RADIO_SETUP:
            loDivider = RF_LODIVIDER_MASK & setupCommand->ble5.loDivider;
            break;

        case CMD_PROP_RADIO_DIV_SETUP:
            loDivider = RF_LODIVIDER_MASK & setupCommand->prop_div.loDivider;
            break;

        default:
            break;
    }

    return loDivider != 0;
}

/*
 *  ======== rfDriverCallbackAntennaSwitching ========
 *  Configure the BDE module RF switch for the active RF path.
 *
 *  Notes:
 *  - DIO29 is the BDE TX/high-power switch control.
 *  - DIO30 is the BDE RX/standard Sub-1 GHz path switch control.
 */
void rfDriverCallbackAntennaSwitching(RF_Handle client,
                                      RF_GlobalEvent events,
                                      void *arg)
{
    if (events & RF_GlobalEventRadioSetup) {
        RF_RadioSetup *setupCommand = (RF_RadioSetup *)arg;

        bdeRfSwitchOff();

        if (!bdeRadioSetupIsSub1GHz(setupCommand)) {
            return;
        }

        RF_TxPowerTable_PAType paType =
            (RF_TxPowerTable_PAType)RF_getTxPower(client).paType;

        if (paType == RF_TxPowerTable_HighPA) {
            /*
             * BDE high-power dynamic switching:
             *   TX: DIO29 = 1, DIO30 = 0
             *   RX: DIO29 = 0, DIO30 = 1
             */
            GPIO_setConfigAndMux(BDE_RF_SWITCH_TX_PIN,
                                 GPIO_CFG_OUTPUT,
                                 IOC_PORT_RFC_GPO3);
            GPIO_setConfigAndMux(BDE_RF_SWITCH_RX_PIN,
                                 GPIO_CFG_OUTPUT,
                                 IOC_PORT_RFC_GPO0);
        }
        else {
            /*
             * Standard Sub-1 GHz path:
             *   DIO29 = 0, DIO30 = 1
             */
            GPIO_setConfigAndMux(BDE_RF_SWITCH_TX_PIN,
                                 GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
                                 IOC_PORT_GPIO);
            GPIO_setConfigAndMux(BDE_RF_SWITCH_RX_PIN,
                                 GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
                                 IOC_PORT_GPIO);
            GPIO_write(BDE_RF_SWITCH_TX_PIN, 0);
            GPIO_write(BDE_RF_SWITCH_RX_PIN, 1);
        }
    }
    else if (events & RF_GlobalEventRadioPowerDown) {
        bdeRfSwitchOff();
    }
}
