/*
 * animate.c
 *
 * LED animation code
 * The behavior of the sun and star LEDs is implemented here
 */

#include "animate.h"
#include <stdlib.h>
#include "sc7a20.h"
#include "system.h"


#define STAR_COUNT              (6)
#define SUN_LIGHT_REST_VALUE    (105)
#define SUN_LIGHT_RANDOM_RANGE  (70)
#define SUN_LIGHT_AVG_MAX       (500)
#define ACCEL_ACTIVITY_MIN      (256)
#define ANI_TICKS_PER_SEC       (32)
#define SLEEP_TIME              (900UL * ANI_TICKS_PER_SEC)
#define STAR_POWER_TIME         (2UL * ANI_TICKS_PER_SEC)


/* Animations data structure */

struct sAni {
    bool accel_available;
    int32_t sun_light_avg;
    int32_t sun_light;
    int32_t last_accel_mag;
    uint32_t time_since_activity;
    uint32_t time_star_power;
    int32_t star_activity;
    uint8_t star_mask;
} volatile static ani;


/* Fast Square root algorithm
 * Fractional parts of the answer are discarded
 * Source - https://stackoverflow.com/a/1101217
 * Posted by Craig McQueen, modified by community
 * Retrieved 2026-10-26, License - CC BY-SA 4.0 */

static uint32_t SquareRoot(uint32_t input)
{
    uint32_t op  = input;
    uint32_t res = 0;
    uint32_t one = 1UL << 30;

    while (one > op)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (op >= res + one)
        {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

/* Read the accelerometer magnitude
 * We use only magnitude because otherwise rotation has a
 * disproportionately large effect and we are more interested in
 * responding to steps, taps and shakes */

static uint32_t ReadAccelMag(void)
{
    if (!ani.accel_available) {
        return 0;
    }

    int32_t x, y, z;
    ReadSC7A20XYZ(&x, &y, &z);

    return SquareRoot((x * x) + (y * y) + (z * z));
}

/* Notify the animation we woke up */

void WakeAnimation(void)
{
    ani.sun_light_avg = SUN_LIGHT_REST_VALUE;
    if (ani.accel_available) {
        ani.star_activity = RAND_MAX / 32;
        ani.time_since_activity = 0;
        ani.last_accel_mag = ReadAccelMag();
    } else {
        ani.star_activity = RAND_MAX / 512;
    }
}

/* Initialize animation system */

void InitAnimation(void)
{
    ani.accel_available = InitSC7A20();
    WakeAnimation();
}

/* Execute an animation step for sun and stars */

void ExecuteAnimationStep(void)
{
    int32_t accel_mag_change_clipped = 0;

    /* Accelerometer based effects */
    if (ani.accel_available) {
        /* Is new accelerometer data available? */
        if (SC7A20HasNewXYZ()) {
            /* Read new data */
            int32_t new_accel_mag = ReadAccelMag();
            /* Calculate accelerometer magnitude difference compared to
            * last sample */
            int32_t accel_mag_change = abs(new_accel_mag - ani.last_accel_mag);
            ani.last_accel_mag = new_accel_mag;
            /* Clip small changes, we want a minimum level of activity */
            accel_mag_change_clipped = accel_mag_change - ACCEL_ACTIVITY_MIN;
            if (accel_mag_change_clipped < 0) {
                accel_mag_change_clipped = 0;
            }
        }

        /* Count our time since activity */
        ani.time_since_activity++;
        /* Do we have accelerometer activity? */
        if (accel_mag_change_clipped > 0) {
            /* Reset the activity timer */
            ani.time_since_activity = 0;
        }
        
        /* Increase the sun light average based on acceleromter activity */
        ani.sun_light_avg += accel_mag_change_clipped >> 4;
        if (ani.sun_light_avg > SUN_LIGHT_AVG_MAX) {
            ani.sun_light_avg = SUN_LIGHT_AVG_MAX;
        }

        /* Increase star sparkle activity based on accelerometer activity */
        ani.star_activity += (accel_mag_change_clipped * RAND_MAX) >> 7;
        if (ani.star_activity > (RAND_MAX / 32)) {
            ani.star_activity = RAND_MAX / 32;
        }
    }

    /* Sun light animation
     * Random walk averaging around the current sun light average */
    if (rand() > (RAND_MAX / 2) && ani.sun_light_avg) {
        ani.sun_light++;
    } else {
        ani.sun_light--;
    }
    if (ani.sun_light < (ani.sun_light_avg - (SUN_LIGHT_RANDOM_RANGE / 8)) &&
            rand() < (RAND_MAX / 4)) {
        ani.sun_light++;
    }
    if (ani.sun_light > (ani.sun_light_avg + (SUN_LIGHT_RANDOM_RANGE / 8)) &&
            rand() < (RAND_MAX / 4)) {
        ani.sun_light--;
    }
    if (ani.sun_light < (ani.sun_light_avg - (SUN_LIGHT_RANDOM_RANGE / 2))) {
        ani.sun_light++;
    }
    if (ani.sun_light > (ani.sun_light_avg + (SUN_LIGHT_RANDOM_RANGE / 2))) {
        ani.sun_light--;
    }
    if (ani.sun_light > 0xFF) {
        ani.sun_light = 0xFF;
    }
    if (ani.sun_light < 0) {
        ani.sun_light = 0;
    }

    /* If the sun average can be changed due to accelerometer input, settle
     * it back toward the default, or to zero if we need to go to sleep due
     * to inactivity */
    if (ani.accel_available && (ani.sun_light_avg > SUN_LIGHT_REST_VALUE ||
            (ani.time_since_activity >= SLEEP_TIME && ani.sun_light_avg))) {
        ani.sun_light_avg--;
    }

    /* Create a mask of stars to randomly sparkle based on activity level */
    ani.star_mask = 0;
    for (int i = 0; i < STAR_COUNT; i++) {
        ani.star_mask <<= 1;
        if (rand() < ani.star_activity) {
            ani.star_mask |= 0x01;
        }
    }

    /* Slowly decrease star activity if we have accelerometer input to
     * trigger it */
    if (ani.accel_available) {
        ani.star_activity -= (RAND_MAX / 8192) + 1;
        if (ani.star_activity < 0) {
            ani.star_activity = 0;
        }
    }

    /* Turn boost converter for stars on for a while if we need to sparkle
     * any or have accelerometer activity */
    if (ani.star_mask || accel_mag_change_clipped) {
        ani.time_star_power = 0;
    } else if (ani.time_star_power < STAR_POWER_TIME) {
        ani.time_star_power++;
    }
    PowerStars(ani.time_star_power < STAR_POWER_TIME);
}

/* Return the current sun light value */

uint8_t GetSunLightLevel(void)
{
    return (uint8_t)ani.sun_light;
}

/* Return a bit mask of stars to sparkle */

uint8_t GetStarSparkleMask(void)
{
    return ani.star_mask;
}

/* Report whether an LED animation is active */

bool IsAnimationActive(void)
{
    return ani.sun_light_avg || ani.sun_light;
}
