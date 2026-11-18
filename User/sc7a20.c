/*
 * sc7a20.c
 *
 * SC7A20 driver code
 */

#include "sc7a20.h"
#include <stdbool.h>
#include "system.h"


/* Register constants */

#define ACCEL_ADDR          0x19

#define REG_WHO_AM_I        0x0F
#define REG_CTRL_REG1       0x20
#define REG_CTRL_REG2       0x21
#define REG_CTRL_REG3       0x22
#define REG_CTRL_REG4       0x23
#define REG_CTRL_REG5       0x24
#define REG_CTRL_REG6       0x25
#define REG_STATUS_REG      0x27
#define REG_OUT_X_L         0x28
#define REG_OUT_X_H         0x29
#define REG_OUT_Y_L         0x2A
#define REG_OUT_Y_H         0x2B
#define REG_OUT_Z_L         0x2C
#define REG_OUT_Z_H         0x2D
#define REG_INT1_CFG        0x30
#define REG_INT1_SRC        0x31
#define REG_INT1_THS        0x32
#define REG_INT1_DURATION   0x33

#define DEVICE_ID           0x11    /* WHO_AM_I expected value */


/* Initialize the accelerometer */

bool InitSC7A20(void)
{
    /* Can we read the device ID correctly? */
    if (I2CRead(ACCEL_ADDR, REG_WHO_AM_I) == DEVICE_ID) {
        /* Turn off all interrupts */
        I2CWrite(ACCEL_ADDR, REG_CTRL_REG3, 0x00);
        /* Low power mode, 25 Hz data rate, XYZ enabled */
        I2CWrite(ACCEL_ADDR, REG_CTRL_REG1, 0x3F);

        return true;
    }

    return false;
}

/* Set fast or slow data rate */

void SC7A20SetFastData(bool on)
{
    /* Low power mode, 25 Hz or 1 Hz data rate, XYZ enabled */
    I2CWrite(ACCEL_ADDR, REG_CTRL_REG1, on ? 0x3F : 0x1F);
}

/* Check if there's new XYZ data */

bool SC7A20HasNewXYZ(void)
{
    return (I2CRead(ACCEL_ADDR, REG_STATUS_REG) & 0x08) != 0;
}

/* Read accelerometer XYZ values */

void ReadSC7A20XYZ(int32_t *x, int32_t *y, int32_t *z)
{
    if (x) {
        *x = (int32_t)((int16_t)I2CRead16(ACCEL_ADDR, REG_OUT_X_L) >> 4);
    }
    if (y) {
        *y = (int32_t)((int16_t)I2CRead16(ACCEL_ADDR, REG_OUT_Y_L) >> 4);
    }
    if (z) {
        *z = (int32_t)((int16_t)I2CRead16(ACCEL_ADDR, REG_OUT_Z_L) >> 4);
    }
}
