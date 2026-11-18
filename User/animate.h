/*
 * animate.h
 *
 * LED animation code
 * The behavior of the sun and star LEDs is implemented here
 */

#ifndef _ANIMATE_H_
#define _ANIMATE_H_

#include <stdint.h>
#include <stdbool.h>


/* Initialize animation system */

void InitAnimation(void);

/* Execute an animation step for sun and stars */

void ExecuteAnimationStep(void);

/* Return the current sun light value */

uint8_t GetSunLightLevel(void);

/* Return a bit mask of stars to sparkle */

uint8_t GetStarSparkleMask(void);

/* Report whether an LED animation is active */

bool IsAnimationActive(void);

/* Notify the animation we woke up */

void WakeAnimation(void);


#endif