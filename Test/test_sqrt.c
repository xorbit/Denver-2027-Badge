#include <stdio.h>
#include <math.h>
#include <stdint.h>

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

int main(void)
{
    uint32_t i = 0;
    do {
	if (i % 10000000UL == 0) {
            printf("... %u ...\n", i);
        }
        uint32_t test_val = SquareRoot(i);
        uint32_t ref_val = (uint32_t)sqrt(i);
	if (test_val != ref_val) {
            printf("%u != %u\n", test_val, ref_val);
        }
    }
    while (++i != 0UL);

    return 0;
}
