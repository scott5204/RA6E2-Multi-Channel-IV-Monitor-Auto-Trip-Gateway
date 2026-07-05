#include "ina226.h"

// 定义用于中断回调的标志位
volatile bool g_i2c_tx_complete = false;
volatile bool g_i2c_rx_complete = false;

// 简单的超时机制，防止 I2C 总线死锁
#define I2C_TIMEOUT_MAX 0xFFFFF 
#define INA226_REG_SHUNT_VOLT 0x01 // 分流电压寄存器
/**
 * @brief I2C 硬件中断回调函数
 * @note 必须在 FSP 的 I2C Master 配置中将 Callback 命名为 i2c_master_callback
 */
void i2c_master_callback(i2c_master_callback_args_t *p_args)
{
    if (NULL != p_args)
    {
        if (I2C_MASTER_EVENT_TX_COMPLETE == p_args->event) {
            g_i2c_tx_complete = true;
        }
        else if (I2C_MASTER_EVENT_RX_COMPLETE == p_args->event) {
            g_i2c_rx_complete = true;
        }
    }
}

/**
 * @brief 向 INA226 写入 16位 寄存器
 */
fsp_err_t INA226_WriteReg(uint8_t slave_addr, uint8_t reg_addr, uint16_t reg_data)
{
    fsp_err_t err;
    uint32_t timeout = 0;

    // 1. 设置目标从机地址
    err = R_IIC_B_MASTER_SlaveAddressSet(&g_i2c_master0_ctrl, slave_addr, I2C_MASTER_ADDR_MODE_7BIT);
    if (FSP_SUCCESS != err) return err;

    // 2. 准备数据: [寄存器地址], [数据高8位], [数据低8位]
    uint8_t tx_buf[3];
    tx_buf[0] = reg_addr;
    tx_buf[1] = (uint8_t)(reg_data >> 8);
    tx_buf[2] = (uint8_t)(reg_data & 0xFF);

    g_i2c_tx_complete = false;
    err = R_IIC_B_MASTER_Write(&g_i2c_master0_ctrl, tx_buf, 3, false);
    
    // 3. 等待发送完成 (带超时保护)
    if (FSP_SUCCESS == err) {
        while (!g_i2c_tx_complete) {
            if (++timeout > I2C_TIMEOUT_MAX) return FSP_ERR_TIMEOUT;
        }
    }
    return err;
}

/**
 * @brief 从 INA226 读取 16位 寄存器
 */
fsp_err_t INA226_ReadReg(uint8_t slave_addr, uint8_t reg_addr, uint16_t *p_data)
{
    fsp_err_t err;
    uint8_t rx_buf[2] = {0};
    uint32_t timeout = 0;

    // 1. 设置目标从机地址
    err = R_IIC_B_MASTER_SlaveAddressSet(&g_i2c_master0_ctrl, slave_addr, I2C_MASTER_ADDR_MODE_7BIT);
    if (FSP_SUCCESS != err) return err;

    // 2. 发送要读取的寄存器地址 (带 Restart)
    g_i2c_tx_complete = false;
    err = R_IIC_B_MASTER_Write(&g_i2c_master0_ctrl, &reg_addr, 1, true);
    if (FSP_SUCCESS == err) {
        while (!g_i2c_tx_complete) {
            if (++timeout > I2C_TIMEOUT_MAX) return FSP_ERR_TIMEOUT;
        }
    } else {
        return err;
    }

    // 3. 读取 2 个字节数据
    timeout = 0;
    g_i2c_rx_complete = false;
    err = R_IIC_B_MASTER_Read(&g_i2c_master0_ctrl, rx_buf, 2, false);
    if (FSP_SUCCESS == err) {
        while (!g_i2c_rx_complete) {
            if (++timeout > I2C_TIMEOUT_MAX) return FSP_ERR_TIMEOUT;
        }
    } else {
        return err;
    }

    // 4. 拼接数据
    *p_data = (uint16_t)((rx_buf[0] << 8) | rx_buf[1]);
    return FSP_SUCCESS;
}

/**
 * @brief 初始化 INA226
 */
fsp_err_t INA226_Init(uint8_t slave_addr)
{
    // 写入 0x8000 触发内部软件复位
    fsp_err_t err = INA226_WriteReg(slave_addr, INA226_REG_CONFIG, 0x8000);
    if (FSP_SUCCESS != err) return err;

    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS); // 等待复位完成

    // 写入配置参数：默认 0x4127
    // 包含：平均采样次数、电压转换时间等，此默认配置已能满足大部分需求
    err = INA226_WriteReg(slave_addr, INA226_REG_CONFIG, 0x4127);
    
    return err;
}

/**
 * @brief 快速获取总线电压 (单位: V)
 * @return 转换后的浮点电压值，读取失败返回 -1.0f
 */
float INA226_GetBusVoltage(uint8_t slave_addr)
{
    uint16_t raw_data = 0;
    if (FSP_SUCCESS == INA226_ReadReg(slave_addr, INA226_REG_BUS_VOLT, &raw_data)) {
        // 总线电压 LSB = 1.25mV
        return (float)raw_data * 0.00125f;
    }
    return -1.0f; // 错误标志
}

/**
 * @brief 获取总线电流 (单位: A)
 * @note 基于 100mR (0.1R) 采样电阻
 * @return 转换后的浮点电流值 (安培)，读取失败返回 -100.0f
 */
float INA226_GetCurrent(uint8_t slave_addr)
{
    uint16_t raw_data = 0;

    if (FSP_SUCCESS == INA226_ReadReg(slave_addr, INA226_REG_SHUNT_VOLT, &raw_data)) {
        // 1. INA226 的分流电压是 16位有符号数 (2的补码)
        // 必须强转为 int16_t，这样才能正确读取反向流动的负电流
        int16_t signed_raw = (int16_t)raw_data;

        // 2. 原理解析：
        // INA226 分流电压寄存器的固定 LSB = 2.5uV (0.0000025V)
        // 根据欧姆定律: I = U / R
        // 真实电流(A) = (signed_raw * 0.0000025) / 0.1欧姆
        // 简化合并后就是: signed_raw * 0.000025

        return (float)signed_raw * 0.000025f;
    }

    // 错误标志：这里不能像电压那样用 -1.0f，因为真实电流有可能是 -1.0A
    // 对于 100mR 电阻，电流物理极限是 0.8A 左右，所以 -100.0f 是一个安全的错误特征码
    return -100.0f;
}