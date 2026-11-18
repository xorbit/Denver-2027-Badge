/*
 * system.c
 *
 * Configure the microcontroller for the application
 * The goal is to have all microcontroller specific code here
 */

#include "py32f002b_ll_cortex.h"
#include "py32f002b_ll_bus.h"
#include "py32f002b_ll_exti.h"
#include "py32f002b_ll_gpio.h"
#include "py32f002b_ll_i2c.h"
#include "py32f002b_ll_lptim.h"
#include "py32f002b_ll_tim.h"
#include "py32f002b_ll_pwr.h"
#include "py32f002b_ll_rcc.h"
#include "py32f002b_ll_utils.h"
#include "system.h"
#include "cie_light_to_pwm.h"
#include "animate.h"
#include "sc7a20.h"
#include <stdlib.h>


/* System clock frequency in run mode */

#define SYS_CLK_FREQ    (24000000UL)

/* I2C clock frequency */

#define I2C_CLK_FREQ    (100000UL)

/* I2C pins */

#define SCL             LL_GPIO_PIN_3
#define SDA             LL_GPIO_PIN_4

/* LED port A pins */

#define LED1            LL_GPIO_PIN_0
#define LED2            LL_GPIO_PIN_1
#define LED3            LL_GPIO_PIN_3
#define LED4            LL_GPIO_PIN_4
#define LED5            LL_GPIO_PIN_5
#define LED6            LL_GPIO_PIN_6
#define LED7            LL_GPIO_PIN_7
#define LED_COUNT       7

static const uint32_t leds[LED_COUNT] = {
    LED1, LED2, LED3, LED4, LED5, LED6, LED7
};

/* Accelerometer difference in any axis to wake up (0.8 g) */

#define ACCEL_WAKE_DIFF (819)


/* Configure the system clock */

static void SystemClockConfig(void)
{
    /* Set HSI to 24 MHz */
    LL_RCC_HSI_Enable();
    LL_RCC_HSI_SetCalibFreq(LL_RCC_HSICALIBRATION_24MHz);
    while(!LL_RCC_HSI_IsReady())
        ;

    /* AHB clock 24 MHz */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

    /* Use HSI as system clock */
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSISYS);
    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSISYS)
        ;

    /* APB1 clock 24 MHz */
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_Init1msTick(SYS_CLK_FREQ);

    /* Set the system core clock */
    LL_SetSystemCoreClock(SYS_CLK_FREQ);
    /* Adjust flash to clock frequency if necessary */
    LL_SetFlashLatency(SYS_CLK_FREQ);
}

/* Low power timer config */

static void LPTimerConfig(void)
{
    /* Disable LSE otherwise it will consume extra 0.6 uA */
    LL_RCC_LSE_Disable();

    /* Enable LSI and have it clock LPTIM1 */
    LL_RCC_LSI_SetCalibTrimming(LL_RCC_LSICALIBRATION_32768Hz);
    LL_RCC_LSI_Enable();
    while (!LL_RCC_LSI_IsReady())
        ;
    LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM1_CLKSOURCE_LSI);

    /* Turn on the LPTIM1 clock */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPTIM1);

    /* Enable auto reload interrupt */
    NVIC_SetPriority(LPTIM1_IRQn, 0);
    NVIC_EnableIRQ(LPTIM1_IRQn);
    LL_LPTIM_EnableIT_ARRM(LPTIM1);

    /* Set prescaler to /2 */
    LL_LPTIM_SetPrescaler(LPTIM1, LL_LPTIM_PRESCALER_DIV2);

    /* Enable the low power timer */
    LL_LPTIM_Enable(LPTIM1);

    /* Reload value to trigger interrupt every 2 seconds
     * From Puya AN1031E:
     * "LPTIM continuous mode must clear ARRMCF each time before entering
     * Stop and wait for 1 LSI clock cycle*PSC factor. (Approx. 40μs*PSC
     * including programmer execution time.)"
     * ARRMCF gets cleared in our low power timer interrupt, and if the
     * prescaler is low enough, the time we spend reading the accelerometer
     * over I2C is enough to satisfy this requirement.
     * When the prescaler was set to higher values, the low power timing
     * was messed up and stop mode would not go to the low power regulator
     * current values but stayed at ~75 uA. */
    LL_LPTIM_SetAutoReload(LPTIM1, 32768 - 1);

    /* Start the low power timer */
    LL_LPTIM_StartCounter(LPTIM1, LL_LPTIM_OPERATING_MODE_CONTINUOUS);
}

/* Execute stop mode */

static void ExecuteStopMode(void)
{
    static int32_t last_x, last_y, last_z, new_x, new_y, new_z;

    /* Make sure the star LED boost converter is off */
    LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0|LL_GPIO_PIN_1);

    /* Get the current accelerometer readings */
    while (!SC7A20HasNewXYZ())
        ;
    ReadSC7A20XYZ(&new_x, &new_y, &new_z);
    /* Set accelerometer slow data rate */
    SC7A20SetFastData(false);
    /* Stay in stop mode until the accelerometer spots a large
     * enough difference along one of its axes */
    do {
        /* Turn on the clock for PWR control */
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
        /* Stop mode with Low-Power Regulator */
        LL_PWR_SetLprMode(LL_PWR_LPR_MODE_LPR);
        /* Set SLEEPDEEP bit of Cortex System Control Register */
        LL_LPM_EnableDeepSleep();
        /* Request Wait For Interrupt */
        __WFI();
        /* Reset SLEEPDEEP bit of Cortex System Control Register */
        LL_LPM_EnableSleep();

        /* Save the last reading for comparison */
        last_x = new_x;
        last_y = new_y;
        last_z = new_z;
        /* Get the current accelerometer readings */
        if (SC7A20HasNewXYZ()) {
            ReadSC7A20XYZ(&new_x, &new_y, &new_z);
        }
    } while (abs(new_x - last_x) < ACCEL_WAKE_DIFF &&
            abs(new_y - last_y) < ACCEL_WAKE_DIFF &&
            abs(new_z - last_z) < ACCEL_WAKE_DIFF);

    /* Accelerometer back to fast data rate */
    SC7A20SetFastData(true);
    /* Notify the animation engine we woke up */
    WakeAnimation();
}

/* Sleep task - Sleep, use deep sleep (stop mode) if possible */

void SleepTask(void)
{
    /* Are LEDs being animated or on? */
    if (IsAnimationActive() || !LL_GPIO_IsOutputPinSet(GPIOA, 
            LED1|LED2|LED3|LED4|LED5|LED6|LED7)) {
        /* Then light sleep */
        __WFI();
    } else {
        /* Execute stop mode */
        ExecuteStopMode();
    }
}

/* GPIO config */

static void GPIOConfig(void)
{
    /* Turn on the clock for the GPIO blocks */
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA |
                            LL_IOP_GRP1_PERIPH_GPIOB);

    /* Configure PB3 and PB4 for I2C bus (PB3 = SCL, PB4 = SDA) */
    LL_GPIO_SetPinMode(GPIOB, SCL, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOB, SDA, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOB, SCL, LL_GPIO_AF6_I2C1);
    LL_GPIO_SetAFPin_0_7(GPIOB, SDA, LL_GPIO_AF6_I2C1);

    /* Configure LED outputs */
    for (int i = 0; i < LED_COUNT; i++) {
        LL_GPIO_SetPinMode(GPIOA, leds[i], LL_GPIO_MODE_OUTPUT);
        LL_GPIO_SetOutputPin(GPIOA, leds[i]);
    }

    /* 5V control (2 pins for extra drive) */
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_0, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_1, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0|LL_GPIO_PIN_1);

    /* Accelerometer interrupt input pin (not currently used - having
     * it interrupt on movement took about 700 uA, much more than we
     * get when we monitor every 2 s from stop mode) */
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_2, LL_GPIO_MODE_INPUT);
}

/* Turn power for stars on or off */

void PowerStars(bool on)
{
    if (on) {
        LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_0|LL_GPIO_PIN_1);
    } else {
        LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0|LL_GPIO_PIN_1);
    }
}

/* I2C config */

static void I2CConfig(void)
{
    /* Turn on the I2C clock */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);

    /* Disable the I2C peripheral */
    LL_I2C_Disable(I2C1);

    /* Configure I2C speed */
    LL_I2C_ConfigSpeed(I2C1, SYS_CLK_FREQ, I2C_CLK_FREQ, LL_I2C_DUTYCYCLE_2);

    /* Enable the selected I2Cx Peripheral */
    LL_I2C_Enable(I2C1);
}

/* Macros to make it easier to add timeouts to I2C synchronous wait */

#define WAIT_FOR(flag_func, ret_fail)                                   \
    for (volatile uint32_t timeout = 2400; !flag_func; ) {              \
        timeout--;                                                      \
        if (!timeout) return ret_fail;                                  \
    }

#define WAIT_FOR_FAIL_NACK(flag_func, ret_fail)                         \
    for (volatile uint32_t timeout = 2400; !flag_func; ) {              \
        if (LL_I2C_IsActiveFlag_AF(I2C1)) {                             \
            LL_I2C_ClearFlag_AF(I2C1);                                  \
            LL_I2C_GenerateStopCondition(I2C1);                         \
            return ret_fail;                                            \
        }                                                               \
        timeout--;                                                      \
        if (!timeout) return ret_fail;                                  \
    }

/* Low throughput I2C access using polling */
/* I2C write function */

bool I2CWrite(uint8_t addr, uint8_t reg, uint8_t data)
{
    WAIT_FOR(!LL_I2C_IsActiveFlag_BUSY(I2C1), false);
    LL_I2C_GenerateStartCondition(I2C1);
    WAIT_FOR(LL_I2C_IsActiveFlag_SB(I2C1), false);
    LL_I2C_TransmitData8(I2C1, addr << 1);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_ADDR(I2C1), false);
    LL_I2C_ClearFlag_ADDR(I2C1);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_TXE(I2C1), false);
    LL_I2C_TransmitData8(I2C1, reg);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_TXE(I2C1), false);
    LL_I2C_TransmitData8(I2C1, data);
    WAIT_FOR(LL_I2C_IsActiveFlag_BTF(I2C1), false);
    LL_I2C_GenerateStopCondition(I2C1);
    WAIT_FOR(!LL_I2C_IsActiveFlag_BUSY(I2C1), false);
    return true;
}

/* I2C read function */

uint8_t I2CRead(uint8_t addr, uint8_t reg)
{
    WAIT_FOR(!LL_I2C_IsActiveFlag_BUSY(I2C1), 0xFF);
    LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_ACK);
    LL_I2C_GenerateStartCondition(I2C1);
    WAIT_FOR(LL_I2C_IsActiveFlag_SB(I2C1), 0xFF);
    LL_I2C_TransmitData8(I2C1, addr << 1);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_ADDR(I2C1), 0xFF);
    LL_I2C_ClearFlag_ADDR(I2C1);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_TXE(I2C1), 0xFF);
    LL_I2C_TransmitData8(I2C1, reg);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_TXE(I2C1), 0xFF);
    LL_I2C_GenerateStartCondition(I2C1);
    WAIT_FOR(LL_I2C_IsActiveFlag_SB(I2C1), 0xFF);
    LL_I2C_TransmitData8(I2C1, (addr << 1) | 1);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_ADDR(I2C1), 0xFF);
    LL_I2C_DisableBitPOS(I2C1);
    LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_NACK);
    __disable_irq();
    LL_I2C_ClearFlag_ADDR(I2C1);
    LL_I2C_GenerateStopCondition(I2C1);
    __enable_irq();
    WAIT_FOR(LL_I2C_IsActiveFlag_RXNE(I2C1), 0xFF);
    uint8_t data = LL_I2C_ReceiveData8(I2C1);
    WAIT_FOR(!LL_I2C_IsActiveFlag_BUSY(I2C1), data);
    return data;
}

/* I2C 16-it read function */

uint16_t I2CRead16(uint8_t addr, uint8_t reg)
{
    WAIT_FOR(!LL_I2C_IsActiveFlag_BUSY(I2C1), 0xFF);
    LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_ACK);
    LL_I2C_GenerateStartCondition(I2C1);
    WAIT_FOR(LL_I2C_IsActiveFlag_SB(I2C1), 0xFF);
    LL_I2C_TransmitData8(I2C1, addr << 1);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_ADDR(I2C1), 0xFF);
    LL_I2C_ClearFlag_ADDR(I2C1);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_TXE(I2C1), 0xFF);
    LL_I2C_TransmitData8(I2C1, reg | 0x80); /* MSB auto increment */
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_TXE(I2C1), 0xFF);
    LL_I2C_GenerateStartCondition(I2C1);
    WAIT_FOR(LL_I2C_IsActiveFlag_SB(I2C1), 0xFF);
    LL_I2C_TransmitData8(I2C1, (addr << 1) | 1);
    WAIT_FOR_FAIL_NACK(LL_I2C_IsActiveFlag_ADDR(I2C1), 0xFF);
    LL_I2C_EnableBitPOS(I2C1);
    __disable_irq();
    LL_I2C_ClearFlag_ADDR(I2C1);
    LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_NACK);
    __enable_irq();
    WAIT_FOR(LL_I2C_IsActiveFlag_BTF(I2C1), 0xFF);
    __disable_irq();
    LL_I2C_GenerateStopCondition(I2C1);
    uint16_t data = LL_I2C_ReceiveData8(I2C1);
    __enable_irq();
    data |= ((uint16_t)LL_I2C_ReceiveData8(I2C1)) << 8;
    WAIT_FOR(!LL_I2C_IsActiveFlag_BUSY(I2C1), data);
    return data;
}

/* LED drive timer config */

static void LEDTimerConfig(void)
{
    /* Turn on TIM14 clock */
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM14);

    /* Enable reload and compare 1 interrupts */
    LL_TIM_EnableIT_UPDATE(TIM14);
    LL_TIM_EnableIT_CC1(TIM14);
    NVIC_EnableIRQ(TIM14_IRQn);

    /* Enable the counter */
    LL_TIM_EnableCounter(TIM14);

    /* Reload value 0xFFFFF for about 732 cycles/s at 24 MHz */
    LL_TIM_SetAutoReload(TIM14, 0x7FFF);

    /* Enable timer compare 1 with preload */
    LL_TIM_CC_EnableChannel(TIM14, LL_TIM_CHANNEL_CH1);
    LL_TIM_OC_EnablePreload(TIM14, LL_TIM_CHANNEL_CH1);
}

/* System configuration on startup */

void SystemConfig(void)
{
    SystemClockConfig();
    GPIOConfig();
    I2CConfig();
    LEDTimerConfig();
    LPTimerConfig();
}

/* Low power timer interrupt handler */

void LPTIM1_IRQHandler(void)
{
    /* Clear interrupt flag */
    LL_LPTIM_ClearFLAG_ARRM(LPTIM1);
}

/* Run animation task if scheduled */

static uint8_t star_sparkle_mask = 0;
static bool run_animation = false;

void RunAnimationTask(void)
{
    if (run_animation) {
        run_animation = false;

        /* Update sun and star animations */
        ExecuteAnimationStep();

        /* Update sun brightness, using CIE lightness curve
         * The CIE table was made for 16-bit range, shift one bit
         * because we have 15-bit timer range */
        LL_TIM_OC_SetCompareCH1(TIM14,
                    CIE_LIGHTNESS_TO_PWM_LUT[GetSunLightLevel()] >> 1);

        /* Update stars */
        star_sparkle_mask = GetStarSparkleMask();
    }
}

/* LED drive timer interrupt handler */

void TIM14_IRQHandler(void)
{
    static uint8_t led_mask = 0;
    static uint8_t ani_schedule_count;

    /* Timer compare 1 interrupt - not-low PWM check
     * Check before the timer update check when PWM values are not low,
     * in that case the timer update might have fired already before we
     * had a chance to service the timer update */
    if ((LL_TIM_OC_GetCompareCH1(TIM14) & 0xFF00) && 
            LL_TIM_IsActiveFlag_CC1(TIM14)) {
        /* Clear the interrupt flag */
        LL_TIM_ClearFlag_CC1(TIM14);

        /* Turn off "sun" LED */
        led_mask &= ~LED1;
    }

    /* Update / overflow interrupt (can override timer compare) */
    if (LL_TIM_IsActiveFlag_UPDATE(TIM14)) {
        /* Clear the interrupt flag */
        LL_TIM_ClearFlag_UPDATE(TIM14);

        /* Start with all LEDs off, then figure out which to turn on */
        led_mask = 0;

        /* Turn on "sun" LED if it's supposed to light up at all */
        if (LL_TIM_OC_GetCompareCH1(TIM14)) {
            led_mask |= LED1;
        }

        /* Convert star mask to LED mask (pins not all contiguous) */
        led_mask |= (star_sparkle_mask & 0xFE) << 2;
        led_mask |= (star_sparkle_mask & 0x01) << 1;
        /* Stars flash only one cycle so clear star mask until
         * next animation update */
        star_sparkle_mask = 0;

        /* Count to see if it's time to schedule an animation update */
        if (++ani_schedule_count >= 23) {
            ani_schedule_count = 0;
            /* Set flag to execute animation */
            run_animation = true;
        }
    }

    /* Timer compare 1 interrupt - low PWM check
     * Check after the timer update check when PWM values are low, in that
     * case the timer compare might have expired already */
    if (!(LL_TIM_OC_GetCompareCH1(TIM14) & 0xFF00) && 
            LL_TIM_IsActiveFlag_CC1(TIM14)) {
        /* Clear the interrupt flag */
        LL_TIM_ClearFlag_CC1(TIM14);

        /* Turn off "sun" LED */
        led_mask &= ~LED1;
    }

    /* Set new LED state (inverted because we pull IO low to turn LED on) */
    LL_GPIO_WriteOutputPort(GPIOA, ~led_mask);
}
