# Embedded Linux Project

## Overview
This repository contains the source code and documentation for an **Embedded Linux project**, developed as part of the **SETI - Embedded Linux** course. The project focuses on **Linux kernel compilation, device driver development, user-kernel communication, and I2C-based peripheral interaction**, all within an **ARM-based emulation environment**.

A key aspect of this project is the **simulation of an ARM Cortex-A9 processor using QEMU**, allowing the deployment and testing of **low-level Linux systems** and **custom kernel drivers** on an **embedded ARM architecture**.

---

## Key Features
- **ARM Cortex-A9 Emulation & Linux Kernel Compilation**  
  - Compiled a **Linux 5.15.6 kernel** for an **ARMv7-A architecture**.  
  - Simulated an **ARM CoreTile Express A9x4 Cortex-A9-based expansion board** using **QEMU**.  
  - Used a **custom QEMU version** with an integrated **ADXL345 accelerometer emulation**.  

- **Custom I2C Device Driver (pilote_i2c)**  
  - Developed a **Linux kernel driver** for the **ADXL345 accelerometer**.  
  - Implemented **probe and remove functions** using the Linux **I2C driver model**.  
  - Modified the **Device Tree** for proper driver discovery.  

- **User-Kernel Communication via Misc Framework**  
  - Exposed device data to user space using a **character device interface**.  
  - Implemented **read and ioctl functions** for efficient user-kernel interaction.  

- **Interrupt Handling & FIFO Buffering**  
  - Configured **hardware interrupts** to efficiently collect accelerometer data.  
  - Managed a **FIFO buffer** to store sensor readings asynchronously.  

- **Concurrency & Multi-Threading Support**  
  - Implemented **thread synchronization mechanisms** for multi-process access.  
  - Used **spinlocks** to ensure safe concurrent access to shared resources.  

---

## Project Structure
📁 embedded-linux-project │── 📂 initramfs_busybox # Initramfs with BusyBox for a minimal Linux root filesystem │── 📂 initramfs_simple # Basic initramfs setup for QEMU booting │── 📂 pilote_i2c # Source code for the ADXL345 I2C device driver │── 📄 .gitignore # Git ignore rules for unnecessary files │── 📄 qemu-system-arm # QEMU binary and configuration files │── 📄 README.md # Project documentation (this file)

---

## Getting Started

### **Prerequisites**
- **QEMU (ARM Cortex-A9 emulation)**
- **Linux Kernel 5.15.6**
- **ARM Cross-Compiler** (`arm-linux-gnueabi-gcc`)
- **BusyBox** (for minimal root filesystem)
- **Build Essentials** (`make`, `gcc`, `binutils`)

### **Installation & Usage**

1. **Clone the Repository**
   ```sh
   git clone https://github.com/coiti4/embedded-linux-project.git
   cd embedded-linux-project

2. **Compile the Linux Kernel for ARM**
   ```sh
   cd kernel
   make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- zImage dtbs modules

3. **Start QEMU with ARM Cortex-A9 Emulation**
   ```sh
   ./qemu-system-arm -M vexpress-a9 -kernel zImage -dtb vexpress-v2p-ca9.dtb -nographic

4. **Build and Load the ARM I2C Device Driver**
   ```sh
   cd pilote_i2c
   make
   insmod adxl345.ko

5. **Test with the User-Space Application**
   ```sh
   ./user_app/main

---

## Results

Successfully compiled and booted Linux on an ARM Cortex-A9 platform.
Developed a functional I2C device driver for the ADXL345 accelerometer.
Implemented interrupt-driven data collection with an efficient FIFO buffer.
Integrated concurrent access control mechanisms for multi-threaded execution.

---

## Future Improvements

Enhance the driver with additional configuration options for power management.
Implement DMA support for optimized data transfer.
Expand user-space utilities to provide a more comprehensive interface.

---

## Contributors

Agustín Coitinho
Arthur Ruback
Irene Asensio
  
## License

This project is licensed under the MIT License – feel free to modify and use it as needed.
