# Phase 6: Distributed Edge Node — CAN Bus Integration (Attempted)

## Goal

Expand the system architecture into a physical multi-node network by migrating the reflex sensor to a **Raspberry Pi Zero 2 W** edge device, communicating real-time punch telemetry back to the **Jetson Nano** master node via a physical **CAN Bus** (CAN-H/CAN-L) differential pair.

**Hardware used:**
- 2× HiLetgo MCP2515 CAN Bus Module (TJA1050 transceiver, 8MHz crystal)
- Raspberry Pi Zero 2 W (edge node, SocketCAN via `mcp251x` kernel driver)
- Jetson Nano (master node, raw SPI via `/dev/spidev0.1`)
- CAN-H / CAN-L twisted pair between both modules

---

## What Was Achieved

- Successfully loaded the `mcp251x` kernel driver on the Pi Zero W by configuring `/boot/firmware/config.txt` with the correct device tree overlay:
  ```
  dtoverlay=mcp2515-can1,oscillator=8000000,interrupt=5,spimaxfrequency=1000000
  ```
- Confirmed MCP2515 initialization via `dmesg`:
  ```
  mcp251x spi0.1 can0: MCP2515 successfully initialized.
  ```
- Brought up `can0` on the Pi at 500kbps using SocketCAN
- Migrated the Pi's edge node from raw SPI register writes to the **SocketCAN** kernel interface (`PF_CAN` socket), which is the correct Linux-native approach for CAN communication
- Confirmed the Pi's MCP2515 was correctly handing off CAN frames to its TJA1050 transceiver via oscilloscope

---

## Root Cause of Failure — Oscilloscope Analysis

Two oscilloscope measurements were taken to isolate where the signal was dying.

### Measurement 1 — TXD Pin (MCP2515 → TJA1050 input)

<img width="4032" height="3024" alt="IMG_5561" src="https://github.com/user-attachments/assets/20d28230-eccd-4be5-86ea-222ee8eb6169" />

| Metric | Value |
|--------|-------|
| VMAX | 3.25V |
| VPP | 0.00V |
| FREQ | 0.00Hz |

A flat 3.25V line on TXD confirmed the MCP2515 chip itself was outputting correctly at full logic voltage. The digital signal was successfully leaving the controller and entering the transceiver. **Software and SPI communication were confirmed to be working.**

### Measurement 2 — CAN-H Terminal Block (physical bus output)

<img width="4032" height="3024" alt="IMG_5563" src="https://github.com/user-attachments/assets/f85ac10d-42cd-4478-88a9-256ca8bafeab" />

| Metric | Value |
|--------|-------|
| VMAX | 1.50V |
| VPP | 0.40V |
| FREQ | 2.17KHz |

A healthy CAN-H line should typically swing between **2.5V and 3.5V**, a full 1V differential swing. Instead, CAN-H only reached **1.50V** with a weak 0.40V peak-to-peak swing.

### Diagnosis

The **TJA1050 transceiver needs a minimum supply voltage of 4.75V** to drive the CAN bus to proper voltage levels. Both modules were powered from **3.3V**, which is below the TJA1050's operating threshold. The transceiver was able to receive the digital signal from the MCP2515 but lacked the supply voltage to drive CAN-H and CAN-L to their correct differential levels. The Jetson's MCP2515 could not decode the weakened signal, causing the Pi to accumulate TX errors and enter **BUS-OFF** state.

```
MCP2515 (3.3V logic) ──TXD──▶ TJA1050 (needs 5V supply) ──CAN-H──▶ [TOO WEAK] ──▶ Jetson never receives anything!
```

The fix would be to power both MCP2515 modules from **5V** while keeping all SPI data lines at 3.3V. This was not implemented due to worries of potentially damaging hardware.

---

## Pivot Decision

Due to the hardware voltage constraints and the time required to rewire and re-validate the physical CAN bus setup, **Phase 6 was deprioritized** in favor of advancing the core system functionality. The distributed edge node architecture remains a valid future expansion, and the software groundwork (SocketCAN integration on the Pi, MCP2515 kernel driver configuration) is complete and documented here for future reference.
