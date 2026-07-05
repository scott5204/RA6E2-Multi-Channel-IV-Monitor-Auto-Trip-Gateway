#include <apps.h>
#include "ina226.h"
#include "bsp_uart.h"
void FAN_STATE_SET(uint8_t fanNAME,uint8_t fanSTATE)
{
    if (fanNAME == 1)
    {
        if (fanSTATE == 1)
        {
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_03_PIN_02, BSP_IO_LEVEL_HIGH);
        }
        else
        {
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_03_PIN_02, BSP_IO_LEVEL_LOW);
        }
    }
    else if (fanNAME == 2)
    {
        if (fanSTATE == 1)
        {
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_03_PIN_01, BSP_IO_LEVEL_HIGH);
        }
        else
        {
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_03_PIN_01, BSP_IO_LEVEL_LOW);
        }
    }
}
void FAN_SPEED_SET(uint8_t fanNAME, uint8_t fanSPEED)
{
    if (fanSPEED > 100) {
        fanSPEED = 100;
    }

    uint32_t actual_output_percent;
    if ((fanNAME == 1 && fan1_polarity) || (fanNAME == 2 && fan2_polarity)) {
        actual_output_percent = fanSPEED;           /* 极性勾选：不反转 */
    } else {
        actual_output_percent = 100 - fanSPEED;     /* 默认：反转 */
    }

    timer_ctrl_t * p_timer_ctrl = NULL;
    if (fanNAME == 1) {
        p_timer_ctrl = &g_timer0_ctrl;
    }
    else if (fanNAME == 2) {
        p_timer_ctrl = &g_timer1_ctrl;
    }
    else {
        return;
    }
    timer_info_t info;
    R_GPT_InfoGet(p_timer_ctrl, &info);
    uint32_t current_period = info.period_counts;
    uint32_t duty_cycle_counts = (current_period * actual_output_percent) / 100;

    if (duty_cycle_counts >= current_period) {
        duty_cycle_counts = current_period - 1;
    }

    R_GPT_DutyCycleSet(p_timer_ctrl, duty_cycle_counts, GPT_IO_PIN_GTIOCB);
}