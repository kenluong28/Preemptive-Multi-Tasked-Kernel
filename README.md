# Preemptive Multi-Tasked Kernel

## Overview

 The project involves implementing a real-time executive (RTX) that supports task scheduling, memory management, inter-task communication, and priority-based preemption on the Intel DE1-SoC FPGA board.

 ## Features

 1. Preemptive Multi-tasking
    - Implements priority-based task scheduling.
    - Supports task creation, termination, and priority adjustment.
    - Utilizes strict-priority scheduling with FIFO for tasks of equal priority.

1. Dynamic Memory Management
   - Implements first-fit memory allocation.
   - Supports dynamic allocation and deallocation.
   - Implements external fragmentation analysis.

2. Inter-task Communication
   - Implements mailbox-based inter-task communication.
   - Supports message passing using structured buffers.
   - Uses priority-based message handling.

3. Hardware and Platform Integration
   - Developed for the Intel DE1-SoC FPGA board.
   - Uses Cortex-A9 dual-core processor with privileged/unprivileged execution.

## Installation and Setup

### Prerequisites
- ARM Development Studio (ARM DS)
- Quartus Prime Software (for FPGA board configuration)
- GitLab for version control
- UART serial terminal (PuTTY or Minicom)

### Cloning the Repository
```
git clone <repository_url>
cd Preemptive-Multi-tasked-Kernel
```

### Building the Kernel

Navigate to the RTX directory and build the kernel:
```
cd RTX
make
```

### Running on DE1-SoC FPGA
1. Power on the DE1-SoC FPGA board.
2. Load the hardware configuration:
    ```
    cd DE1-SoC
    ./program.bat
    ```
3. Connect to the board using UART Terminal (e.g., PuTTY).
4. Run the RTX kernel and observe task scheduling and memory management in action.

## Usage
### Running the Kernel

To start the kernel, use:
```
./RTX.launch
```

### Testing Task Management
To create and manage tasks, modify app/user_tasks.c and compile:
```
make clean && make
```
