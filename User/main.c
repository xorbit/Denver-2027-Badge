/*
 * Denver 2027 Badge main application code
 */

#include "main.h"
#include "system.h"
#include "animate.h"

int main(void)
{
    /* Configure the hardware */
    SystemConfig();
    /* Initialize the animation engine */
    InitAnimation();

    /* Endless main loop */
    for(;;)
    {
        /* Run the animation task when scheduled */
        RunAnimationTask();
        /* Handle sleep (light or deep sleep) */
        SleepTask();
    }
}

