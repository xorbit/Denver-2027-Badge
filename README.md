# Denver 2027 Badge

This repo contains the source code for the Denver 2027 sparkling LED badge.

The code runs on a Puya PY32F002B low cost microcontroller, queries a low cost
SC7A20 accelerometer, drives LEDs and manages low power modes to extend coin
cell battery life.

The project is based on the [py32f0-template](https://github.com/IOsetting/py32f0-template)
project customized for my needs.  It uses the GNU Arm Embedded Toolchain,
development in the VSCode IDE and programming devices through a J-Link.

## Firmware behavior

- On power-up, the sun will glow and the stars will sparkle.  Left alone, the
sun glow will settle at a low level with some random wandering in brightness
and the stars will stay off.  Average current in this state is 2 ~ 2.5 mA for
an expected on-time of about 4 days when running from a 240 mAh CR2032 coin
cell.
- While on, the accelerometer is sampled at 25 Hz.  The code calculates the
accelerometer magnitude vector and tries to ignore the angle.  This makes the
badge sensitive to walking steps and badge taps but mostly ignores orientation.
If a magnitude change of > 1/4 g is detected, the sun brightness and star
sparkle intensity are increased in proportion to the detected change.
This will make the badge more active when walking around and mostly inert with
just a low sun glow when sitting still.
- If no accelerometer change of > 1/4 g is detected for more than 15 minutes,
the badge goes to deep sleep.
- In deep sleep, the system only draws 2.7 uA.  The accelerometer is switched
to 1 Hz sample rate and the micro wakes up every 2 seconds for about 2 ms to
read the accelerometer.  Adding these events in, the average current in deep
sleep is ~ 4.2 uA for an expected battery life of about 6.5 years when powered
from a 240 mAh CR2032 coin cell.  Actual battery life will depend on how much
time is spent on, and how much time in deep sleep.
- To wake back up from deep sleep, a change of 0.8 g needs to be seen on any
of the accelerometer axes.  This does not ignore orientation and one of the
easiest ways to wake the badge from deep sleep is to turn it at least 90
degrees or flip it over.  The high g threshold and long sampling interval
makes the badge insensitive to vibration and movement in deep sleep, reducing
unintentional wake-ups when in storage or during transport.
