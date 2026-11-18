/*
 * system.h
 *
 * Configure the microcontroller for the application
 * The goal is to have all microcontroller specific code here
 */

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <stdint.h>
#include <stdbool.h>


 /* System configuration on startup */

void SystemConfig(void);

/* Turn power for stars on or off */

void PowerStars(bool on);

/* I2C write function */

bool I2CWrite(uint8_t addr, uint8_t reg, uint8_t data);

/* I2C read function */

uint8_t I2CRead(uint8_t addr, uint8_t reg);

/* I2C 16-it read function */

uint16_t I2CRead16(uint8_t addr, uint8_t reg);

/* Sleep task - Sleep, use deep sleep (stop mode) if possible */

void SleepTask(void);

/* Run animation task if scheduled */

void RunAnimationTask(void);


 #endif