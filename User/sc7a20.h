/*
 * sc7a20.c
 *
 * SC7A20 driver code
 */

#ifndef _SC7A20_H_
#define _SC7A20_H_

#include <stdbool.h>
#include <stdint.h>


/* Initialize the accelerometer */

bool InitSC7A20(void);

/* Set fast or slow data rate */

void SC7A20SetFastData(bool on);

/* Check if there's new XYZ data */

bool SC7A20HasNewXYZ(void);

/* Read accelerometer XYZ values */

void ReadSC7A20XYZ(int32_t *x, int32_t *y, int32_t *z);


#endif