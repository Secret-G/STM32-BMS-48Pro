#ifndef __BQ34Z100_REG_H
#define __BQ34Z100_REG_H

#include "stdint.h"

/*
 * BQ34Z100-G1 I2C Address
 *
 * 当前 soft_i2c1 的 dev_addr 使用 7 位地址，
 * 底层会自动左移并拼接读写位。
 *
 * 7-bit address : 0x55
 * 8-bit write   : 0xAA
 * 8-bit read    : 0xAB
 */
#define BQ34Z100_I2C_ADDR                 0x55U

/* Standard Data Commands */
#define BQ34Z100_CMD_CONTROL              0x00U

#define BQ34Z100_CMD_STATE_OF_CHARGE      0x02U
#define BQ34Z100_CMD_MAX_ERROR            0x03U

#define BQ34Z100_CMD_REMAINING_CAPACITY   0x04U
#define BQ34Z100_CMD_FULL_CHARGE_CAPACITY 0x06U
#define BQ34Z100_CMD_VOLTAGE              0x08U
#define BQ34Z100_CMD_AVERAGE_CURRENT      0x0AU
#define BQ34Z100_CMD_TEMPERATURE          0x0CU
#define BQ34Z100_CMD_FLAGS                0x0EU
#define BQ34Z100_CMD_CURRENT              0x10U
#define BQ34Z100_CMD_FLAGS_B              0x12U

/* Extended Data Commands */
#define BQ34Z100_CMD_CYCLE_COUNT          0x2CU
#define BQ34Z100_CMD_STATE_OF_HEALTH      0x2EU

#endif