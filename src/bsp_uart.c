#include "bsp_uart.h"
#include "apps.h"

/* hal_entry.c 中定义的全局变量 */
extern volatile float volt1;
extern volatile float volt2;
extern volatile float curr1;
extern volatile float curr2;

/* ========== TX 标志 ========== */
volatile bool uart0_tx_complete = false;

/* ========== 当前页码 ========== */
volatile uint8_t current_page = 0;

/* ========== RX 接收 ========== */
#define RX_BUF_SIZE 64
static uint8_t rx_buf[RX_BUF_SIZE];
static uint32_t rx_len = 0;

/* ========== 风扇参数 ========== */
volatile uint8_t fan1_duty = 0;
volatile uint8_t fan2_duty = 0;
volatile uint8_t fan1_polarity = 0;
volatile uint8_t fan2_polarity = 0;
volatile bool fan_update_request = false;

/* ========== get 响应（主循环轮询用） ========== */
volatile uint8_t get_resp_val = 0;
volatile bool get_resp_ready = false;

/* ========== 调试：实时转速（CLion Live Watch 查看） ========== */
volatile uint16_t dbg_speed1 = 0;
volatile uint16_t dbg_speed2 = 0;

/* ========== 发送原始数据（阻塞） ========== */
static void send_bytes(const uint8_t *data, uint32_t len)
{
    uart0_tx_complete = false;
    fsp_err_t err = R_SCI_UART_Write(&g_uart0_ctrl, data, len);
    if (FSP_SUCCESS != err) return;

    uint32_t timeout = 0xFFFFF;
    while (!uart0_tx_complete) {
        if (--timeout == 0) return;
    }
}

/* ========== 发送指令 + \xff\xff\xff ========== */
void UART0_SendCmd(const char *cmd)
{
    uint32_t len = strlen(cmd);
    if (len == 0 || len > 60) return;

    uint8_t buf[64];
    uint32_t i;
    for (i = 0; i < len; i++) {
        buf[i] = (uint8_t)cmd[i];
    }
    buf[i++] = 0xFF;
    buf[i++] = 0xFF;
    buf[i++] = 0xFF;
    send_bytes(buf, i);
}

/* ========== 更新串口屏显示 ========== */
void UART0_UpdateDisplay(void)
{
    /* 每次先发 sendme 查询当前页码 */
    UART0_SendCmd("sendme");
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

    /* 不在第0页，跳过显示更新 */
    if (current_page != 0) return;

    char buf[48];
    int len;

    /* t1 = 电压 (volt1) */
    if (volt1 < 0.0f) {
        UART0_SendCmd("t1.txt=\"ERR\"");
    } else {
        uint16_t cv = (uint16_t)(volt1 * 100.0f + 0.5f);
        len = sprintf(buf, "t1.txt=\"%u.%02u\"", cv / 100, (unsigned int)(cv % 100));
        buf[len] = 0xFF; buf[len+1] = 0xFF; buf[len+2] = 0xFF;
        send_bytes((uint8_t*)buf, (uint32_t)len + 3);
    }

    /* t3 = 电流1 (curr1) */
    if (curr1 < 0.0f) {
        UART0_SendCmd("t3.txt=\"ERR\"");
    } else {
        uint16_t ma = (uint16_t)(curr1 * 1000.0f + 0.5f);
        len = sprintf(buf, "t3.txt=\"%u.%03u\"", ma / 1000, (unsigned int)(ma % 1000));
        buf[len] = 0xFF; buf[len+1] = 0xFF; buf[len+2] = 0xFF;
        send_bytes((uint8_t*)buf, (uint32_t)len + 3);
    }

    /* t5 = 电流2 (curr2) */
    if (curr2 < 0.0f) {
        UART0_SendCmd("t5.txt=\"ERR\"");
    } else {
        uint16_t ma = (uint16_t)(curr2 * 1000.0f + 0.5f);
        len = sprintf(buf, "t5.txt=\"%u.%03u\"", ma / 1000, (unsigned int)(ma % 1000));
        buf[len] = 0xFF; buf[len+1] = 0xFF; buf[len+2] = 0xFF;
        send_bytes((uint8_t*)buf, (uint32_t)len + 3);
    }
}

/* ========== 初始化 UART0 (SCI0, 115200 8N1) ========== */
void UART0_Init(void)
{
    fsp_err_t err = R_SCI_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);
    if (FSP_SUCCESS != err) {
        return;
    }
}

/* ========== 帧超时保护 ========== */
static uint32_t rx_stale_counter = 0;
static uint32_t prev_rx_len = 0;

void UART0_RxStaleCheck(void)
{
    if (rx_len > 0) {
        if (rx_len == prev_rx_len) {
            rx_stale_counter++;
            if (rx_stale_counter >= 3) {   // 3 个主循环周期 ≈ 150ms 无新字节
                rx_len = 0;
                rx_stale_counter = 0;
            }
        } else {
            rx_stale_counter = 0;  // 有新字节，重置计数器
        }
    } else {
        rx_stale_counter = 0;
    }
    prev_rx_len = rx_len;
}

/* ========== 串口中断回调 ========== */
void uart0_callback(uart_callback_args_t *p_args)
{
    if (p_args->event == UART_EVENT_TX_COMPLETE) {
        uart0_tx_complete = true;
        return;
    }

    if (p_args->event == UART_EVENT_RX_CHAR) {
        uint8_t byte = (uint8_t)p_args->data;

        if (rx_len < RX_BUF_SIZE) {
            rx_buf[rx_len++] = byte;
        }
        if (rx_len >= RX_BUF_SIZE) {
            rx_len = 0;
            return;
        }

        /* 检测帧尾 \xff\xff\xff */
        if (rx_len >= 3 &&
            rx_buf[rx_len - 1] == 0xFF &&
            rx_buf[rx_len - 2] == 0xFF &&
            rx_buf[rx_len - 3] == 0xFF) {

            uint32_t msg_len = rx_len - 3;

            /* 页码响应: 66 PAGE FF FF FF (5 字节) */
            if (msg_len == 2 && rx_buf[0] == 0x66) {
                current_page = rx_buf[1];
            }

            /* 触发指令: 65 01 05 01 */
            else if (msg_len == 4 &&
                rx_buf[0] == 0x65 &&
                rx_buf[1] == 0x01 &&
                rx_buf[2] == 0x05 &&
                rx_buf[3] == 0x01) {
                fan_update_request = true;
            }

            /* get 响应: 71 VAL 00 00 00 FF FF FF (8 字节) */
            else if (rx_len == 8 && rx_buf[0] == 0x71) {
                get_resp_val = rx_buf[1];
                get_resp_ready = true;
            }

            rx_len = 0;
        }
    }
}
