#ifndef INA226_H_
#define INA226_H_

#include "hal_data.h"

/* INA226 I2C 硬件地址 (根据 A0, A1 引脚接法决定) */
#define INA226_ADDR_GND_GND     0x40  // A1=GND, A0=GND
#define INA226_ADDR_GND_VCC     0x41  // A1=GND, A0=VCC
#define INA226_ADDR_GND_SDA     0x42  // A1=GND, A0=SDA
#define INA226_ADDR_GND_SCL     0x43  // A1=GND, A0=SCL

/* INA226 寄存器地址映射 */
#define INA226_REG_CONFIG       0x00  // 配置寄存器
#define INA226_REG_SHUNT_VOLT   0x01  // 分流电压寄存器
#define INA226_REG_BUS_VOLT     0x02  // 总线电压寄存器
#define INA226_REG_POWER        0x03  // 功率寄存器
#define INA226_REG_CURRENT      0x04  // 电流寄存器
#define INA226_REG_CALIBRATION  0x05  // 校准寄存器

/* 函数声明 */
fsp_err_t INA226_Init(uint8_t slave_addr);
fsp_err_t INA226_WriteReg(uint8_t slave_addr, uint8_t reg_addr, uint16_t reg_data);
fsp_err_t INA226_ReadReg(uint8_t slave_addr, uint8_t reg_addr, uint16_t *p_data);
float     INA226_GetBusVoltage(uint8_t slave_addr);
float INA226_GetCurrent(uint8_t slave_addr);

#endif /* INA226_H_ */