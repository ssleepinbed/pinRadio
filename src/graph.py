import sys
import numpy as np
import matplotlib.pyplot as plt

def plot_gpio_waveform_with_reflections(f0, L):

    # Parameters
#    f0 = 305e6          # base GPIO frequency (Hz)
    V0 = 1.0            # logic amplitude
#    L = 0.25            # wire length in meters
    v = 2e8             # propagation speed in wire (m/s)
    Gamma = 1.0         # reflection coefficient (1 = open, -1 = short)
    harmonics = 5       # number of odd harmonics to include

    if(L <= 0):
        L = v/(6*f0) # Optimal length for max 3rd harmonic

    # Time array (enough periods to see waveform)
    T = 1 / f0
    dt = T / 1000
    t = np.arange(0, 10*T, dt)

    # Forward voltage (square wave Fourier series)
    V_forward = np.zeros_like(t)
    for n in range(harmonics):
        k = 2*n + 1
        V_forward += (4*V0)/(np.pi*k) * np.sin(2*np.pi*k*f0*t)

    # Round-trip delay
    tau = 2*L/v

    # Reflected voltage (delayed and scaled)
    V_reflected = np.zeros_like(t)
    for n in range(harmonics):
        k = 2*n + 1
        phi = 2*np.pi*k*f0*tau   # phase shift
        V_reflected += Gamma * (4*V0)/(np.pi*k) * np.sin(2*np.pi*k*f0*t - phi)

    # Total voltage
    V_total = V_forward + V_reflected

    # Plot time-domain waveform
    plt.figure(figsize=(12,5))
    plt.plot(t*1e9, V_total)
    plt.title(f"GPIO waveform with reflections (f0={f0/1e6} MHz, L={L*100} cm)")
    plt.xlabel("Time (ns)")
    plt.ylabel("Voltage (a.u.)")
    plt.grid(True)

    # Plot spectrum
    plt.figure(figsize=(12,5))
    fft_vals = np.fft.fft(V_total)
    fft_freq = np.fft.fftfreq(len(t), dt)
    idx = fft_freq > 0
    plt.stem(fft_freq[idx]/1e6, np.abs(fft_vals[idx]), basefmt=" ")
    plt.title("Spectrum of GPIO pin signal with reflections")
    plt.xlabel("Frequency (MHz)")
    plt.ylabel("Amplitude (a.u.)")
    plt.xlim(0, f0*10/1e6)
    plt.grid(True)

    plt.show()


if __name__ == "__main__":
    plot_gpio_waveform_with_reflections(sys.argv[1], sys.argv[2])