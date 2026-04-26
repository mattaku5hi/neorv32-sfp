# gowin-riscv Project

## 1. Description
Software running on neorv32 software RISC-V core, integrated in Gowin GW2A FPGA.  
This branch contains a variant of a FreeRTOS-based project.

## 2. Getting started
### 2.1 Install the dependencies 
``` bash
sudo apt update
sudo apt install cmake
```
### 2.2 Clone the project
Please, clone spictl.git project to your local repository by means of
SSH:
```bash
git clone git@gitlab.independence.digital:printer/gowin-riscv.git
```
```
Move to a project folder:
```bash
cd </path/to/gowin-riscv project/>
```
Initialize and update all the required submodules:
```bash
git submodule update --init --recursive
```

### 2.3 Make sure you have riscv gcc toolchain properly installed
I.e.
```bash
ls -la /opt/riscv
```
If not, then download it:
```bash
git clone https://github.com/riscv/riscv-gnu-toolchain
```
And build:
```bash
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv --with-arch=rv32i --with-abi=ilp32
make -j$(nproc)
```


### 2.4 Build the project:
Build the release project
```bash
cd </path/to/gowin-riscv project/>
./tools/build_release.sh
```
Build the debug project
```bash
cd </path/to/gowin-riscv project/>
./tools/build_debug.sh
```

### 3. Project structure
## 3.1 Image generator 
Image generator target executable is _neo_sigillum_image_gen_.  
ELF executable is build via GCC toolchain set up on a host machine.  
Image generator is utility that parses target NeoRV32 binaries and creates a header for them.  
The header is 12 bytes: 4-byte magic word (hex speaking), 4-byte image length, 4-byte checksum. 

## 3.2 Image loader 
Image loader target executable is _neo_sigillum_image_load_.  
ELF executable is build via GCC toolchain set up on a host machine.  
Image loader is utility that loads images to a SPI flash based on printer management board via UART.  
It deals with _Bootloader_ firmware running on FPGA.
It automatically scans for FTDI devices and chooses the second (_B_) to handle the programming job.  
To use it, __libftdi__ must be properly installed (check project Wiki page).

## 3.3 Bootloader 
Bootloader target 'executable' is _neo_sigillum_boot.vhd_.  
Binary file is built by GCC RISC-V toolchain.  
.vhd FPGA module is created by _image_gen_ utility.    
It must be then build together with FPGA modules as a single firmware image.  
Implements image loading, verifying and executing from SPI and image update via UART.

## 3.4 Demo 
Demo target executable is _neo_sigillum_demo_exe.bin_.  
Binary file is built by GCC RISC-V toolchain.  
'Exe' file is a binary file with a 12-bytes header attached to it. The header is added by _image_gen_ utility.  
It may be loaded and executed from Gowin external SPI flash memory or from Gowin RAM.  
Implements basic FreeRTOS project with threads, queue and leds blinking.

## 3.5 Application 
Application target executable is _neo_sigillum_exe.bin_.  
Binary file is built by GCC RISC-V toolchain.  
'Exe' file is a binary file with a 12-bytes header attached to it. The header is added by _image_gen_ utility.  
It may be loaded and executed from Gowin external SPI flash memory or from Gowin RAM.  
Implements algorithms of printer processes management.