import serial
import serial.tools.list_ports
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import sys

# ---- CONFIG ----
BAUD_RATE   = 115200
WINDOW_SIZE = 256
VDD         = 3.3
ADC_MAX     = 16383

measurements = {
    'freq_ref': 0.0, 'period_ref': 0, 'amp_ref': 0.0,
    'freq_test': 0.0, 'period_test': 0, 'amp_test': 0.0,
    'phase': 0.0,
}

# ---- PORT ----
def find_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if 'USB' in p.description or 'UART' in p.description:
            return p.device
    for i, p in enumerate(ports):
        print(f"{i}: {p.device} - {p.description}")
    return ports[int(input("Select port: "))].device

ser = serial.Serial(find_port(), BAUD_RATE, timeout=1)

# ---- SERIAL PARSE ----
def read_serial():
    while ser.in_waiting > 0:
        try:
            line = ser.readline().decode('ascii', errors='ignore').strip()
            if line.startswith('S:'):
                r, t = line[2:].split(',')
                xdata_ref.append(read_serial.t)
                ydata_ref.append((int(r) / ADC_MAX) * VDD)
                xdata_test.append(read_serial.t)
                ydata_test.append((int(t) / ADC_MAX) * VDD)
                read_serial.t += 1
            elif line.startswith('M:'):
                p = line[2:].split(',')
                if len(p) == 7:
                    measurements['freq_ref']    = float(p[0])
                    measurements['period_ref']  = float(p[1])
                    measurements['amp_ref']     = float(p[2])
                    measurements['freq_test']   = float(p[3])
                    measurements['period_test'] = float(p[4])
                    measurements['amp_test']    = float(p[5])
                    measurements['phase']       = float(p[6])
        except (ValueError, UnicodeDecodeError):
            pass

read_serial.t = 0

# ---- PLOT SETUP ----
def on_close(event):
    ser.close()
    sys.exit(0)

fig, (ax, ax_info) = plt.subplots(2, 1, figsize=(12, 7),
                                   gridspec_kw={'height_ratios': [3, 1]},
                                   facecolor='white')
fig.canvas.mpl_connect('close_event', on_close)

ax.set_facecolor('white')
ax.set_ylim(-0.1, VDD + 0.3)
ax.set_xlim(0, WINDOW_SIZE)
ax.grid()
ax.set_ylabel('Voltage (V)')
ax.set_xlabel('Sample')
ax.set_title('EFM8 Oscilloscope')

xdata_ref,  ydata_ref  = [], []
xdata_test, ydata_test = [], []

line_ref,  = ax.plot([], [], lw=2, color='blue')
line_test, = ax.plot([], [], lw=2, color='red')

# phase arrow + label
arrow = ax.annotate('', xy=(0, VDD*0.85), xytext=(0, VDD*0.85),
                    arrowprops=dict(arrowstyle='<->', color='black', lw=1.5))
arrow_label = ax.text(0, VDD * 0.9, '', ha='center', fontsize=9, color='black')

# ---- INFO PANEL ----
ax_info.set_facecolor('white')
ax_info.axis('off')
cols = [0.01, 0.18, 0.34, 0.52, 0.72]
for col, h in zip(cols, ['', 'FREQUENCY', 'PERIOD', 'AMPLITUDE', 'PHASE']):
    ax_info.text(col, 0.95, h, transform=ax_info.transAxes, fontsize=8,
                 fontweight='bold', va='top', color='black')

ax_info.text(cols[0], 0.55, 'REF',  transform=ax_info.transAxes, color='blue',  fontsize=10, fontweight='bold', va='center')
ax_info.text(cols[0], 0.15, 'TEST', transform=ax_info.transAxes, color='red',   fontsize=10, fontweight='bold', va='center')

t_fr  = ax_info.text(cols[1], 0.55, '---', transform=ax_info.transAxes, color='blue', fontsize=10, va='center')
t_pr  = ax_info.text(cols[2], 0.55, '---', transform=ax_info.transAxes, color='blue', fontsize=10, va='center')
t_ar  = ax_info.text(cols[3], 0.55, '---', transform=ax_info.transAxes, color='blue', fontsize=10, va='center')
t_ph  = ax_info.text(cols[4], 0.55, '---', transform=ax_info.transAxes, color='black', fontsize=10, va='center', fontweight='bold')
t_ft  = ax_info.text(cols[1], 0.15, '---', transform=ax_info.transAxes, color='red',  fontsize=10, va='center')
t_pt  = ax_info.text(cols[2], 0.15, '---', transform=ax_info.transAxes, color='red',  fontsize=10, va='center')
t_at  = ax_info.text(cols[3], 0.15, '---', transform=ax_info.transAxes, color='red',  fontsize=10, va='center')

plt.tight_layout()

# ---- UPDATE ----
def run(frame):
    read_serial()

    t = read_serial.t
    if t > WINDOW_SIZE:
        ax.set_xlim(t - WINDOW_SIZE, t)

    line_ref.set_data(xdata_ref,  ydata_ref)
    line_test.set_data(xdata_test, ydata_test)

    m = measurements

    # update phase arrow: find zero cross of ref, offset by phase
    if len(ydata_ref) > 10 and m['freq_ref'] > 0:
        arr = np.array(ydata_ref[-WINDOW_SIZE:])
        xs  = np.array(xdata_ref[-WINDOW_SIZE:])
        mid = VDD / 2.0
        cross = next((i for i in range(1, len(arr)) if arr[i-1] < mid <= arr[i]), None)
        if cross is not None:
            spd = BAUD_RATE / (10 * m['freq_ref'])
            x0  = xs[cross]
            x1  = x0 + (m['phase'] / 360.0) * spd
            y_a = VDD * 0.88
            arrow.xy     = (x1, y_a)
            arrow.xytext = (x0, y_a)
            arrow_label.set_position(((x0 + x1) / 2, y_a + 0.06))
            arrow_label.set_text(f"{m['phase']:+.1f}°")

    t_fr.set_text(f"{m['freq_ref']:.1f} Hz")
    t_pr.set_text(f"{m['period_ref']} ms")
    t_ar.set_text(f"{m['amp_ref']:.3f} V")
    t_ft.set_text(f"{m['freq_test']:.1f} Hz")
    t_pt.set_text(f"{m['period_test']} ms")
    t_at.set_text(f"{m['amp_test']:.3f} V")
    t_ph.set_text(f"{m['phase']:+.1f} deg")

    return line_ref, line_test

ani = animation.FuncAnimation(fig, run, blit=False, interval=50, repeat=False)
plt.show()