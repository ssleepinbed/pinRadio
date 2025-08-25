# pinRadio
### A small hardware/software hack that allows sending RF waves without any radio hardware, using a pin + traces on a board as a transmission line!

<sup>*Made to work on a Raspberry Pi 3B+*</sup>

The goal is to send signals at an arbitrary frequency, up to 1 GHz, to potentially give a device with no RF hardware to communicate wirelessly.
***Why** are we doing this? Because it's interesting. Believe me, this has almost no practical uses.*

# How it works

At the end of the day, pins are just conductors driven high/low by transistors. Toggling them rapidly creates a time-varying current. The magic is in **getting them to toggle really fast.**
Switching the state of a pin with a loop from a program is no good, it is just not fast enough. 

In order to get a really high frequency current, we apply three tricks:

## (1) Abusing clocks

### How the pins and clock work - BCM2837 (skip if familiar)

On a Raspberry Pi 3, the BCM2837 has general purpose clocks which can output to GPIO pins. This is the approach I used for this device, and for other devices it will be different,
but the jist is to find something that can generate a **square wave** and output to a pin.

In order to get the clock to output to a GPIO pin (GPIO 4 in this case), we need to 'hijack' the pin from the device and route the output of the clock to the pin.
The pins of the BCM2837 are managed by **32bit registers,** with each register managing 3 pins, where the **function of each pin is represented by 3 bits.**
Each pin can have up to 7 functions: Pull, and ALT0-5. Here is a list of 3-bit values and the functions they correspond to:

| Bits | Function|
-------|---------|
| 000 | Input    |
| 001 | Output   |
| 010 | ALT5     |
| ... | ...      |
| 101 | ALT0     |
------------------

GPIO Pin 4 can route to the clock if it is set to ALT0.

The clocks are also managed by registers. These registers are 32 bits long, with **the first register corresponding to GPCLKn_CTL**, and the **second register corresponding to GPCLKn_DIV.**
The clock always outputs a **square wave** at a frequency $f_c$:

$$\text{\Huge{$ f_c = \frac{f_s}{I + \frac{F}{4096}} $}}$$

$f_s$ is determined by bits 4-6 of the **GPCLKn_CTL** register (Clock Control of Clock #n):

| Bits | Function|
-------|---------|
| 000 | GND      |
| 001 | 19.2MHz  |
| 010 | test     |
| 011 | PLL A    |
| 100 | PLL C    |
| 101 | PLL D    |
| 110 | HDMI     |
------------------

Numbers **I** and **F** are determined by the **GPCLKn_DIV** register. This is a 32 bit register, which stores I and F as 12 bit numbers, one after the other:

| Bits | Function|
-------|---------|
| 0:11 | F       |
| 12:23| I       |
| 24:31| PSWD    |
------------------

### Accessing the registers

On the BCM2837, the clock control and pin registers are mapped into the SoC physical address space, in the **peripherals section** at offset **0x3F000000** (**BASE**).
GPIO registers are at an offset **0x200000** from **BASE**, while clock registers are at an offset **0x101000** from **BASE.**
We can actually hijack these registers, and edit them from user-space, by **memory mapping their address to virtual memory,** and then **casting a pointer that points to their values,** which we can later dereference.

## (2) Square waves

By simply mapping the physical memory and putting values into the register, **we can generate a very weak wave on a pin**:
![](https://i.postimg.cc/T2BY3Yk4/1a-33-3mhz.png)

Attaching an antenna (simply wire) to this pin boosts the signal, **showing us leaking harmonics and reflected signals all over the spectrum**:
[![](https://i.postimg.cc/kgpTkzMK/2-30ghz.png)](https://postimg.cc/87Ldvy6z)

But this is not all bad...

As the frequency $f_s$ is limited, at most 19.2MHz (though we use 500MHz from $PLL_D$), we cannot generate square waves at a very high frequency, but we do not need to.
The square wave is compromised of many superimposed sinusoidal waves; **the first wave at a target frequency $f_c$**, with the rest being **odd harmonics with frequency $n*f_c$ (n = 3, 5, ...).**
The power that a harmonic carries gets smaller and smaller as n goes up, but even with the second harmonic, **we can achieve frequencies 5 times larger than our clock frequency.**
This is how, **by setting a frequency of ~305MHz, we can get a signal at ~916 MHz:**
[![](https://i.postimg.cc/zD7Ss60v/916-MHz-no-reflection-b.png)](https://postimg.cc/dD7ydHMc)
More precise values require more precise tuning.
The signal is there, but now we face another problem: though it is noticable, it is weak, and as it stands, this signal couldn't travel more than a few meters in my testing.
This makes sense, **but we can make it stronger.**

$$\text{\Huge{$ P_r ≈ \frac{((2 pi L)/	λ)^2}{6pi} * \frac{V^2}{R_s} $}}$$

## (3) Reflections

The traces, pin, and wire attached to the pin act as a **transmission line** for our RF signal. Each transmission line has a **reflection coefficient!**
If we leave our 'antenna' open, with its impedance approaching infinity, we will ensure that at least a part of our wave will be reflected back through the wire.
Now, here's the trick: If we make the length of the transmission line (by attaching a conductive material to our pin; a wire) exactly right, so that the peak of the reflected wave travels through the line, reflects back, and its peak exits **at the same time as our original signal, or one of its harmonics' peaks, they'll add up.**
We can get the spacing ($Δf$; the difference between $f_c$ and the frequency of the reflected wave) with:
$$\text{\Huge{$ Δf = \frac{v}{2L} $}}$$

And if we match $f_c + Δf$ with the frequency of one of our harmonics, $n * f_c$, they'll add up constructively.
Below you can see that in action, with the image on the left being a signal without a reflection adding to it, and the image on the right being a signal with reflection adding to it:
[![No reflection](https://i.postimg.cc/8zYmcv66/916-MHz-no-reflection-2.png)](https://postimg.cc/rDSrYKgV)  |  [![reflection](https://i.postimg.cc/R0nQH07b/916-MHz-with-reflection-b.png)](https://postimg.cc/JyMkwMdb)

# Conclusion

Combining the above mentioned effects, I have been able to **get the signal to reliably travel under 100 meters.** I will add more features to this little program as time goes on, the goal is to send LoRa packets with it! I hope you like it!
