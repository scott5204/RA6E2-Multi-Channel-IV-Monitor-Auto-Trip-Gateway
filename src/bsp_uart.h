#ifndef __BSP_UART_H_
#define __BSP_UART_H_
#include "hal_data.h"
#include <stdio.h>
#include <string.h>

void UART0_Init(void);
void UART0_SendCmd(const char *cmd);
void UART0_UpdateDisplay(void);
void uart0_callback(uart_callback_args_t *p_args);
void UART0_RxStaleCheck(void);

extern volatile uint8_t fan1_duty;
extern volatile uint8_t fan2_duty;
extern volatile uint8_t fan1_polarity;
extern volatile uint8_t fan2_polarity;
extern volatile bool fan_update_request;
extern volatile uint8_t get_resp_val;
extern volatile bool get_resp_ready;
extern volatile uint16_t dbg_speed1;
extern volatile uint16_t dbg_speed2;
extern volatile uint8_t current_page;

#endif
