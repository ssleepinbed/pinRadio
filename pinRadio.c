#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>

// Address constants
#define BCM2835_BASE 0x3F000000 // This is the address where the BCM2835 peripherals are mapped from
#define GPIO_BASE   (BCM2835_BASE + 0x200000) // GPIO pins are mapped here
#define CLOCK_BASE  (BCM2835_BASE + 0x101000) // Clock registers are mapped here

// Block sizes are incorrect on some datasheets. They are 4*1024, not 1024.
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

#define GZ_CLK_BUSY    (1 << 7)
#define GZ_PASSWD      (0x5A << 24) // Raspberry Pi control password

volatile unsigned *gpio; // We'll map the GPIO pin's register to this pointer
volatile unsigned *clk;  // We'll map the clock's register to this pointer
// Keep these volatile or the compiler will optimize them.

int mem_fd;


// These are only set once, so no need to be volatile
void *gpio_map;
void *clk_map;

void cleanup(int sig);


int main() {
    double targetFrequency;
    printf("Enter target frequency (Hz): ");
    if(scanf("%lf", &targetFrequency) != 1) { perror("Invalid input"); return 1;}

        // We first open /dev/mem.
    if((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) { // Small remark here. O_SYNC immediately flushes data from CPU cache, which we want,
        perror("Couldn't open /dev/mem. Consider sudo!");  // But sometimes glitches can still appear, because ARM CPUs use write buffers :(
        return -1;
    }

    gpio_map = mmap(NULL, BLOCK_SIZE, PORT_READ|PORT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
    if(gpio_map == MAP_FAILED) { perror("mmap couldn't map GPIO"); return -1; }

    clk_map = mmap(NULL, BLOCK_SIZE, PORT_READ|PORT_WRITE, MAP_SHARED, mem_fd, CLOCK_BASE);
    if(clk_map == MAP_FAILED) { perror("mmap couldn't map Clock"); return -1; }

    // We can technically cast these to volatile uint32_t* instead of unsigned*, but it's the same size.
    // We can also technically cast the original gpip_map and clk_map without using a separate variable, but this is cleaner.
    gpio = (volatile unsigned *)gpio_map;
    clk = (volatile unsigned *)clk_map;

    signal(SIGINT, cleanup); // Register cleanup handler for Ctrl+C
    signal(SIGTERM, cleanup); // Register cleanup handler for kill command. I just like to do it this way.

    // On this processor, each gpio register is 32 bits wide and controls 10 pins.
    // Each pin has 3 bits for function select. We set GPIO pin 4 to ALT0 (GPCLK0).
    // So we set bits 12-14 to 100. (000 input, 001 output, 100-010 alt functions)
    int gpfsel = *(gpio + 0);
    gpfsel &= ~(7 << 12);
    gpfsel |=  (4 << 12); // Set GPIO pin 4 to ALT0 (GPCLK0)
    *(gpio + 0) = gpfsel;

    int harmonic = 3;
    double baseFrequency = targetFrequency / harmonic;
    double pllFrequency = 500e6;
    int divisor = (int)(pllFrequency / baseFrequency);
    // Divisor cant be less than 2. Hardware limit.
    if(divisor < 2 || divisor > 4095) { fprintf(stderr, "Divisor %d out of range (2-4095). Try a different frequency.\n", divisor); cleanup(0); }

    printf("Target frequency: %.2f MHz, base frequency: %.2f MHz, harmonic: %d, divisor: %d\n", targetFrequency/1e6, baseFrequency/1e6, harmonic, divisor);

    *(clk + (0x70/4)) = GZ_PASSWD | (1 << 5);
    usleep(10);
    while (*clk((0x70/4)) & GZ_CLK_BUSY) {}

    // Divisor
    *(clk + (0x74/4)) = GZ_PASSWD | (divisor << 12);

    // Enable clock, source=PLLD (6)
    *(clk + (0x70/4)) = GZ_PASSWD | 0x10 | 6;

    printf("Running! Ctrl+C to stop!\n");

    while(1) { pause(); } // Waiting..waiting..waiting...

    cleanup(0);
    return 0;

}


void cleanup(int sig) {

    // We basically do the reverse of what we did previously.

    *(clk + (0x70/4)) = GZ_PASSWD | (1 << 5); // Disable clock
    usleep(10);

    if(gpio_map) { munmap(gpio_map, BLOCK_SIZE); }
    if(clk_map) { munmap(clk_map, BLOCK_SIZE); }
    if(mem_fd >= 0) { close(mem_fd); }

    printf("Exiting. Byebye!\n");
    exit(0);
}