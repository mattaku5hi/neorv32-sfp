// ================================================================================ //
// The NEORV32 RISC-V Processor - https://github.com/stnolting/neorv32              //
// Copyright (c) NEORV32 contributors.                                              //
// Copyright (c) 2020 - 2024 Stephan Nolting. All rights reserved.                  //
// Licensed under the BSD-3-Clause license, see LICENSE for details.                //
// SPDX-License-Identifier: BSD-3-Clause                                            //
// ================================================================================ //

/**
 * @file bootloader.c
 * @brief DI custom NEORV32 bootloader for printer management PCB
 */
#include <stdbool.h>

#include <neorv32.h>
#include <spi_flash.h>
#include <utils.h>

/**********************************************************************//**
* @name Bootloader configuration (override via console to customize);
* default values are used if not explicitly customized
**************************************************************************/
/**@{*/

/**********************************************************************//**
* Sanity check: Base RV32I ISA only!
**************************************************************************/
#if defined __riscv_atomic || defined __riscv_a || __riscv_b || __riscv_compressed || defined __riscv_c || defined __riscv_mul || defined __riscv_m
#warning In order to allow the bootloader to run on *any* CPU configuration it should be compiled using the base rv32i ISA only.
#endif


/* -------- Boot mode detemination pins -------- */
#ifndef GPIO_BOOT_0
#define GPIO_BOOT_0     0
#endif
#ifndef GPIO_BOOT_1
#define GPIO_BOOT_1     1
#endif

/* -------- Executable header magic bytes -------- */
#ifndef APP_EXE_SIGNATURE
#define APP_EXE_SIGNATURE   0x4788CAFE
#endif


#ifndef BOOT_SPI_FLASH_ATOM                   
#define BOOT_SPI_FLASH_ATOM           SPI_FLASH_PAGE_SIZE
#endif


#ifndef BOOT_READ_EXE_BY_WORD
#define BOOT_READ_EXE_BY_WORD   1
#endif // BOOT_READ_EXE_BY_WORD

#ifndef BOOT_READ_EXE_BY_BYTE
#define BOOT_READ_EXE_BY_BYTE   0
#endif // BOOT_READ_EXE_BY_BYTE

/* -------- Memory layout -------- */

/** Memory base address for the executable */
#ifndef BOOT_RAM_BASE
#define BOOT_RAM_BASE   0x00000000UL
#endif

/* -------- UART interface -------- */

/** Set to 0 to disable UART interface */
#ifndef UART_EN
#define UART_EN 1
#endif

/** UART BAUD rate for serial interface */
#ifndef UART_BAUD
#define UART_BAUD   115200
#endif  

/** Set to 1 to enable UART HW handshaking */
#ifndef UART_HW_HANDSHAKE_EN
#define UART_HW_HANDSHAKE_EN    0
#endif

/* -------- Status LED -------- */

/** Set to 0 to disable bootloader status LED (heart beat) at GPIO.gpio_o(STATUS_LED_PIN) */
#ifndef STATUS_LED_EN
#define STATUS_LED_EN   1
#endif

/** GPIO output pin for high-active bootloader status LED (heart beat) */
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN  0
#endif

#ifndef BOOT_PACKET_TIMEOUT_S
#define BOOT_PACKET_TIMEOUT_S     20
#endif

/* -------- NAND configuration -------- */
#ifndef NAND_EN
#define NAND_EN  1
#endif

/* -------- XIP configuration -------- */

/** Enable XIP boot options */
#ifndef XIP_EN
#define XIP_EN  0
#endif

/* -------- SPI configuration -------- */
/** SPI flash boot base address */
#ifndef BOOT_SPI_BASE
// #define BOOT_SPI_BASE  0x00410000UL
#define BOOT_SPI_BASE  0x00400000UL
#endif

/** SPI flash clock pre-scaler; see #NEORV32_SPI_CTRL_enum */
#ifndef SPI_FLASH_CLK_PRSC
#define SPI_FLASH_CLK_PRSC  CLK_PRSC_2
#endif

/* -------- NAND flash configuration -------- */
#ifndef NAND_BOOT_EN
#define NAND_BOOT_EN    1
#endif

#ifndef NAND_BOOT_BASE 
#define NAND_BOOT_BASE 0x000000000UL
#endif

/**@}*/


/**********************************************************************//**
* Util macros
**************************************************************************/
/**@{*/
/** Actual define-to-string helper */
#define xstr(a) str(a)
/** Internal helper macro */
#define str(a) #a
/* NeoRV32 sleep */
#ifndef BOOT_CPU_SLEEP
#define BOOT_CPU_SLEEP { while(1) { neorv32_cpu_sleep(); } }
#endif
/** Print to UART 0 */
#if defined(UART_EN) && UART_EN == 1
#define BOOT_PRINTSTR(...) neorv32_uart0_puts(__VA_ARGS__)
#define BOOT_PRINTHEX(a) neo_uart_print_hex(NEORV32_UART0, a)
#define BOOT_GETC(a) neorv32_uart0_getc()
#define BOOT_PUTC(a) neorv32_uart0_putc(a)
#else
#define BOOT_PRINTSTR(...)
#define BOOT_PRINTHEX(a)
#define BOOT_GETC(a) 0
#define BOOT_PUTC(a)
#endif
/**@}*/

typedef enum
{
    BOOT_MODE_FLASH_NAND,
    BOOT_MODE_FLASH_SPI,
    BOOT_MODE_UART,
    BOOT_MODE_RESERVED,
    BOOT_MODE_INVALID,

} boot_mode_t;

/**********************************************************************//**
* Error codes
**************************************************************************/
typedef enum
{
    BOOT_OK,
    BOOT_ERROR_HW, // incorrect hardware configuration
    BOOT_ERROR_SIGNATURE, /**< 0: Wrong signature in executable */
    BOOT_ERROR_SIZE_IMAGE, /**< 1: Insufficient instruction memory capacity */
    BOOT_ERROR_SIZE_CHUNK,
    BOOT_ERROR_CHECKSUM_IMAGE, /**< 2: Checksum error in executable */
    BOOT_ERROR_CHECKSUM_CHUNK,
    BOOT_ERROR_FLASH,  /**< 3: SPI flash access error */
    BOOT_ERROR_MODE, // Incorrect boot mode
    BOOT_ERROR_TRANSFER,
    BOOT_ERROR_CHUNK_OFFSET,
    
    BOOT_STATUS_SIZE,
} boot_status_t;

enum boot_response_code_e
{
    BOOT_RESP_OK,

    BOOT_RESP_ERROR_IMAGE_SIGN,
    BOOT_RESP_ERROR_PACKET_TIMEOUT,
    BOOT_RESP_ERROR_IMAGE_LENGTH,
    BOOT_RESP_ERROR_CHUNK_LENGTH,
    BOOT_RESP_ERROR_CHUNK_CHECKSUM,
    BOOT_RESP_ERROR_IMAGE_CHECKSUM,
    BOOT_RESP_ERROR_CHUNK_OFFSET,
    BOOT_RESP_ERROR_PAGE_VERIFY,
    
    BOOT_RESP_ERROR_UNKNOWN,
    
};
typedef unsigned short boot_response_code_t;


typedef enum
{
    EXE_OFFSET_SIGNATURE =  0, /**< Offset in bytes from start to signature (32-bit) */
    EXE_OFFSET_SIZE      =  4, /**< Offset in bytes from start to size (32-bit) */
    EXE_OFFSET_CHECKSUM  =  8, /**< Offset in bytes from start to checksum (32-bit) */
    EXE_OFFSET_DATA      = 12, /**< Offset in bytes from start to data (32-bit) */
} neorv32_exe_offset_t;


typedef union 
{
    uint32_t uint32;
    unsigned char uint8[sizeof(uint32_t)];
} data_read_word_t;

typedef union 
{
    unsigned short uint16;
    unsigned char uint8[sizeof(unsigned short)];
} data_read_subword_t;

typedef struct
{
    uint32_t signature;
    uint32_t size;
    uint32_t checksum;

} boot_image_header_t;


typedef struct
{
    uint32_t offset;
    unsigned short length;
    unsigned short crc;
} boot_packet_header_t;
typedef struct
{
    boot_packet_header_t header;
    uint32_t buffer[BOOT_SPI_FLASH_ATOM / sizeof(uint32_t)]; 
} boot_packet_t;

typedef struct
{
    boot_response_code_t code;
    unsigned short crc;
} boot_response_t;


/**********************************************************************//**
* Static function prototypes
**************************************************************************/
static void __attribute__((interrupt("machine"), aligned(4))) bootloader_trap_handler(void);

static inline uint32_t boot_checksum_add_byte(unsigned char byte, uint32_t checksum);
static inline boot_status_t boot_flash_spi_check(void);
static inline boot_mode_t boot_mode_get(void);
static inline void boot_system_status(boot_status_t code);
static inline void boot_exe_check(boot_status_t status);
static inline uint32_t boot_get_word_uart(void);
static inline uint32_t boot_get_word_spi(uint32_t address);
static inline void boot_response_send(boot_response_t response);

static void boot_init(void);
static void* boot_memcpy(void* pDest, const void* pSrc, size_t length);
static int boot_memcmp(const void* pA, const void* pB, size_t length);
static boot_status_t boot_get_prologue_uart(boot_image_header_t* pHeader);
static boot_status_t boot_get_header_spi(uint32_t exeBase, boot_image_header_t* pHeader);
static boot_status_t boot_exe_spi_process(uint32_t addressSpi, uint32_t addressRam);
static boot_status_t boot_exe_uart_process(uint32_t addressSpi, uint32_t addressRam);
static void __attribute__((noreturn)) boot_exe_start(uint32_t exeBase);
static void __attribute__((noreturn)) boot_system_error(boot_status_t code);


/**********************************************************************//**
* Error messages
**************************************************************************/
const char statusString[BOOT_STATUS_SIZE][16] = 
{
    "OK",
    "ERR_HW",
    "ERR_SIGN",
    "ERR_SIZE_I",
    "ERR_SIZE_C",
    "ERR_CRC_I",
    "ERR_CRC_C",
    "ERR_FLSH",
    "ERR_MODE",
    "ERR_XFER",
    "ERR_OFF_C",
};

/**********************************************************************//**
* Only set during executable fetch (required for capturing STORE BUS-TIMOUT exception).
**************************************************************************/
volatile bool isGettingExe;
uint64_t gettingExeStartTime;
uint64_t ledTogglePeriod;

/**********************************************************************//**
* Bootloader trap handler. Used for the MTIME tick and to capture any other traps.
*
* @note Since we have no runtime environment we have to use the interrupt attribute here.
**************************************************************************/
static void __attribute__((interrupt("machine"), aligned(4))) bootloader_trap_handler(void)
{
    register uint32_t mcause = neorv32_cpu_csr_read(CSR_MCAUSE);

    // Machine timer interrupt
    if(mcause == TRAP_CODE_MTI)
    {
#if defined(STATUS_LED_EN) && STATUS_LED_EN == 1
        neorv32_gpio_pin_toggle(STATUS_LED_PIN); // toggle status LED
#endif
        // set time for next IRQ
        register uint64_t currentTime = neorv32_clint_time_get();
        neorv32_clint_mtimecmp_set(currentTime + ledTogglePeriod);
        // if(isGettingExe && ((gettingExeStartTime - currentTime) >= NEORV32_SYSINFO->CLK * BOOT_PACKET_TIMEOUT_S))
        // {
        //     isGettingExe = false;
        //     BOOT_PRINTSTR("\nTimeout occured!\n");
        //     BOOT_CPU_SLEEP;
        // }
    }

    // Bus store access error during get_exe
    else if((mcause == TRAP_CODE_S_ACCESS) && (isGettingExe))
    {
        boot_system_error(BOOT_ERROR_TRANSFER); // -> seems like executable transfer is pending for too long
    }

    // Anything else (that was not expected); output exception notifier and try to resume
    else
    {
        register uint32_t mepc = neorv32_cpu_csr_read(CSR_MEPC);
#if defined(UART_EN) && UART_EN == 1
        if(neorv32_uart0_available())
        {
            BOOT_PRINTSTR("\nERR_EXC ");
            BOOT_PRINTHEX(mcause);
            BOOT_PUTC(' ');
            BOOT_PRINTHEX(mepc);
            BOOT_PUTC(' ');
            BOOT_PRINTHEX(neorv32_cpu_csr_read(CSR_MTVAL));
            BOOT_PRINTSTR("\n");
        }
#endif
        neorv32_cpu_csr_write(CSR_MEPC, mepc + 4); // advance to next instruction
    }
}


/**********************************************************************//**
* Bootloader main entry
**************************************************************************/
int main(void) 
{
    boot_mode_t bootMode = BOOT_MODE_INVALID;
    boot_status_t status = BOOT_ERROR_HW;

    boot_init();

    bootMode = boot_mode_get();
    switch(bootMode)
    {
        case BOOT_MODE_FLASH_SPI:
        {
            status = boot_exe_spi_process(BOOT_SPI_BASE, BOOT_RAM_BASE);
            if(status == BOOT_OK)
            {
                BOOT_PRINTSTR("OK\n");
                boot_exe_start(BOOT_RAM_BASE);
            }
            else if(status == BOOT_ERROR_SIGNATURE || status == BOOT_ERROR_SIZE_IMAGE || status == BOOT_ERROR_CHECKSUM_IMAGE)
            {
                isGettingExe = false;
                boot_system_status(status);
                BOOT_PRINTSTR("\nERROR! Invalid image!\nPlease, download the correct software image via UART\n");
                status = boot_exe_uart_process(BOOT_SPI_BASE, BOOT_RAM_BASE);
                boot_exe_check(status);
            }
            else
            {
                boot_system_error(status);
            }
            break;
        }
        case BOOT_MODE_UART:
        {
            status = boot_exe_uart_process(BOOT_SPI_BASE, BOOT_RAM_BASE);
            boot_exe_check(status);
            /* If returned then something had gone wrong */
            break;
        }
        case BOOT_MODE_FLASH_NAND:
        {
            BOOT_PRINTSTR("NAND boot mode is currently unavailable!\n");
            /* Ahtung!!! Do not insert break; here */
            status = BOOT_ERROR_MODE;
            break;
        }
        case BOOT_MODE_RESERVED:
        {
            BOOT_PRINTSTR("Reserved boot mode is currently unavailable!\n");
            /* Ahtung!!! Do not insert break; here */
            status = BOOT_ERROR_MODE;
            break;
        }       
        default:
        {
            status = BOOT_ERROR_MODE;
            break;
        }
    }

    /* We will never return from this function */
    boot_system_error(status);
    __builtin_unreachable();
    while (1); // should never be reached
}


static void* boot_memcpy(void* pDest, const void* pSrc, size_t length)
{
    unsigned char* pDestByte = (unsigned char*)pDest;
    const unsigned char* pSrcByte = (const unsigned char*)pSrc;

    for(size_t i = 0; i < length; i++)
    {
        pDestByte[i] = pSrcByte[i];
    }

    return pDest;
}

static int boot_memcmp(const void* pA, const void* pB, size_t length)
{
    const unsigned char* p1 = (const unsigned char *)pA;
    const unsigned char* p2 = (const unsigned char *)pB;

    for(size_t i = 0; i < length; i++)
    {
        if(p1[i] != p2[i])
        {
            return p1[i] - p2[i];
        }
    }
    
    return 0;
}

static unsigned short boot_crc16(const unsigned char* pData, size_t length) 
{
    unsigned short crc = 0xffff; // Initial value
    for(size_t i = 0; i < length; i++)
    {
        crc ^= pData[i];
        for(int j = 0; j < 8; ++j)
        {
            if(crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            } 
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static inline void boot_response_send(boot_response_t response)
{
    for(size_t i = 0; i < sizeof(response); i++)
    {
        BOOT_PUTC(((char*)&response)[i]);
    }

    return;
}

static inline uint32_t boot_checksum_add_byte(unsigned char byte, uint32_t checksum)
{
    register uint32_t sum = (checksum << 8) | byte; 
    sum &= 0xffffffff;

    return sum;
}

/**
 * @brief 
 */
static void boot_init()
{
    isGettingExe = false; // we are not trying to get an executable yet
    ledTogglePeriod = NEORV32_SYSINFO->CLK / 4;

    // configure trap handler (bare-metal, no neorv32 rte available)
    neorv32_cpu_csr_write(CSR_MTVEC, (uint32_t)(&bootloader_trap_handler));

#if defined(UART_EN) && UART_EN == 1
    // setup UART0
    neorv32_uart0_setup(UART_BAUD, 0);
#if defined(UART_HW_HANDSHAKE_EN) && UART_HW_HANDSHAKE_EN == 1
    neorv32_uart0_rtscts_enable();
#endif
#endif

    // ------------------------------------------------
    // Show bootloader intro and system info
    // ------------------------------------------------
    BOOT_PRINTSTR("\n\n<< Digital independence NeoRV32 Bootloader >>\n\n"
                "BLDV: "__DATE__"\nHWV:  ");
    BOOT_PRINTHEX(neorv32_cpu_csr_read(CSR_MIMPID));
    BOOT_PRINTSTR("\nCLK:  ");
    BOOT_PRINTHEX(NEORV32_SYSINFO->CLK);
    BOOT_PRINTSTR("\nIMEM: ");
    BOOT_PRINTHEX((uint32_t)(1 << NEORV32_SYSINFO->MISC[SYSINFO_MISC_IMEM]) & 0xFFFFFFFCUL);
    BOOT_PRINTSTR("\nDMEM: ");
    BOOT_PRINTHEX((uint32_t)(1 << NEORV32_SYSINFO->MISC[SYSINFO_MISC_DMEM]) & 0xFFFFFFFCUL);
    BOOT_PRINTSTR("\n");

#if defined(SPI_EN) && SPI_EN == 1
    // setup SPI for clock-mode 0
    if(neorv32_spi_available() == 1)
    {
        neorv32_spi_setup(SPI_FLASH_CLK_PRSC, 0, 0, 0, 0);
        neorv32_spi_highspeed_enable();
    }
    else
    {
        boot_system_error(BOOT_ERROR_HW);
    }
#endif

#if defined(XIP_EN) && XIP_EN == 1
    // setup XIP: clock divider 0, clock mode 0
    if(neorv32_xip_available())
    {
        neorv32_xip_setup(SPI_FLASH_CLK_PRSC, 0, 0, 0, SPI_FLASH_CMD_READ);
        neorv32_xip_start(SPI_FLASH_ADDR_BYTES);
    }    
    else
    {
        boot_system_error(BOOT_ERROR_HW);
    }
#endif

    if(neorv32_gpio_available() == 0)
    {
        boot_system_error(BOOT_ERROR_HW);
    }

#if defined(STATUS_LED_EN) && STATUS_LED_EN == 1
    // activate status LED, clear all others
    neorv32_gpio_port_set(1 << STATUS_LED_PIN);
#endif

    // Configure machine system timer interrupt
    if(neorv32_clint_available() == 1)
    {
        NEORV32_CLINT->MTIME.uint32[0] = 0;
        NEORV32_CLINT->MTIME.uint32[1] = 0;
        NEORV32_CLINT->MTIMECMP[0].uint32[0] = NEORV32_SYSINFO->CLK/4;
        NEORV32_CLINT->MTIMECMP[0].uint32[1] = 0;
        neorv32_cpu_csr_write(CSR_MIE, 1 << CSR_MIE_MTIE); // activate MTIME IRQ source
        neorv32_cpu_csr_set(CSR_MSTATUS, 1 << CSR_MSTATUS_MIE); // enable machine-mode interrupts
    }
    else
    {
        boot_system_error(BOOT_ERROR_HW);    
    }

    return;
}

/**
 * @brief 
 * @param  
 * @return 
 */
static inline boot_mode_t boot_mode_get(void)
{
    uint32_t mode;
    uint32_t mode_lo = neorv32_gpio_pin_get(0);
    uint32_t mode_hi = neorv32_gpio_pin_get(1) >> 1;
    
    mode = (mode_lo & 0x1) | ((mode_hi & 0x1) << 1);
    
    return (boot_mode_t)mode;
}

static inline boot_status_t boot_flash_spi_check()
{
    /* check if SPI module is implemented and if flash ready (or available at all) */
    if(neorv32_spi_available() == 0 || // SPI module not implemented?
            (neo_spi_flash_check() != 0))
    {
        /* Some hardware problems have occured, need help! */
        return BOOT_ERROR_FLASH;
    }
    else
    {
        return BOOT_OK;
    }   
}

static inline void boot_exe_check(boot_status_t status)
{   
    isGettingExe = false;
    if(status == BOOT_OK)
    {
        return;
    }
    else if(status == BOOT_ERROR_SIGNATURE || status == BOOT_ERROR_SIZE_IMAGE || status == BOOT_ERROR_CHECKSUM_IMAGE ||
        status == BOOT_ERROR_CHECKSUM_CHUNK || status == BOOT_ERROR_SIZE_CHUNK)
    {
        isGettingExe = false;
        boot_system_status(status);
        BOOT_PRINTSTR("\nERROR! Invalid image!\nPlease, restart the device and try again\n");
        BOOT_CPU_SLEEP;
    }
    else
    {
        boot_system_error(status);
    }
}

static inline unsigned short boot_get_subword_uart()
{
    data_read_subword_t data;

    unsigned short i;
    for(i = 0; i < 2; i++)
    {
        data.uint8[i] = (unsigned char)BOOT_GETC();
    }

    return data.uint16;
}

static inline uint32_t boot_get_word_uart()
{
    data_read_word_t data;

    uint32_t i;
    for(i = 0; i < 4; i++)
    {
        data.uint8[i] = (unsigned char)BOOT_GETC();
    }

    return data.uint32;
}

static inline uint32_t boot_get_word_spi(uint32_t address)
{
    data_read_word_t data;

    uint32_t i;
    for(i = 0; i < 4; i++)
    {
        data.uint8[i] = neo_spi_flash_read_byte(address + i); // little-endian byte order
    }

    return data.uint32;
}

static boot_status_t boot_exe_spi_process(uint32_t addressSpi, uint32_t addressRam)
{
    boot_image_header_t appHeader = {0};
    boot_status_t status = BOOT_ERROR_FLASH;
    uint32_t appChecksum = 0;
    bool isFirstPage = true;

    BOOT_PRINTSTR("Booting from SPI flash @");
    BOOT_PRINTHEX(addressSpi);
    BOOT_PRINTSTR(" \n");

    /* check if SPI module is implemented and if flash ready (or available at all) */
    status = boot_flash_spi_check();
    if(status != BOOT_OK)
    {
        return status;
    }
    // isGettingExe = true; // to inform trap handler we were trying to get an executable

#if defined(XIP_EN) && XIP_EN == 1
    BOOT_PRINTSTR(" via XIP...\n");
    /* If using XIP, we may just jump to the application without copying it to RAM */
    addressSpi += XIP_MEM_BASE_ADDRESS; // + sizeof(boot_image_header_t) ? 
    /* We will never return */
#else
    status = boot_get_header_spi(addressSpi, &appHeader);
    if(status != BOOT_OK)
    {
        return status;
    }
    /* We get here if and only if everything is OK */
    uint32_t addressTemp = addressSpi;

    unsigned char* const pRam = (unsigned char* const)addressRam;
    for(size_t i = 0; i < appHeader.size; i += BOOT_SPI_FLASH_ATOM, addressTemp += BOOT_SPI_FLASH_ATOM)
    {
        /* Check if it's last not full page to be read */
        size_t delta = appHeader.size - i;
        size_t length = delta >= BOOT_SPI_FLASH_ATOM ?  BOOT_SPI_FLASH_ATOM : delta;
        /* Ignore the image header */
        if(isFirstPage)
        {
            addressTemp += sizeof(boot_image_header_t);
            length -= sizeof(boot_image_header_t);
        }

        /* Read flash page to RAM */
        unsigned char* const pRamTemp = pRam + i * sizeof(unsigned char);
        neo_spi_flash_read_page(addressTemp, length, pRamTemp);
        /* Update image checksum */
        for(size_t j = 0; j < length / 4; j++)
        {
            uint32_t word = ((uint32_t*)pRamTemp)[j];
            appChecksum += word;
        }

        /* Restore SPI flash address and clear the flag */
        if(isFirstPage)
        {
            addressTemp -= sizeof(boot_image_header_t);
            i -= sizeof(boot_image_header_t);
            isFirstPage = false;
        }
    }
   
    isGettingExe = false;
    BOOT_PRINTSTR("Calc cs: "); BOOT_PRINTHEX(appChecksum); BOOT_PRINTSTR(" \n");
    if(appChecksum + appHeader.checksum != 0)
    {
        return BOOT_ERROR_CHECKSUM_IMAGE;
    }
#endif

    return BOOT_OK;
}

static boot_status_t boot_exe_uart_process(uint32_t addressSpi, uint32_t addressRam)
{
    boot_status_t status = BOOT_ERROR_TRANSFER;
    uint32_t appChecksum = 0;
    /* Begin writing not from begging of page to store a header there later */
    size_t index = sizeof(boot_image_header_t) / sizeof(uint32_t);
    boot_packet_t packet = {0};
    uint32_t pageRead[BOOT_SPI_FLASH_ATOM / sizeof(uint32_t)];
    uint32_t offsetPrev = 0;
    size_t readSize = 0; 
    unsigned short crc;
    boot_image_header_t imageHeader = {0};
    boot_response_t response = {.code = BOOT_RESP_OK};
    response.crc = boot_crc16((const unsigned char*)&(response.code), sizeof(response.code));
    uint32_t addressSpiData = addressSpi;
    uint32_t sectorErasePrev = addressSpiData / SPI_FLASH_SECTOR_SIZE * SPI_FLASH_SECTOR_SIZE;

    BOOT_PRINTSTR("Awaiting NeoRV32 executable via UART... ");

#if defined(SPI_EN) && SPI_EN == 1
    /* check if SPI module is implemented and if flash ready (or available at all) */
    status = boot_flash_spi_check();
    if(status != BOOT_OK)
    {
        return status;
    }
#endif

    /* NeoRV32 lib's uart functions for getting symbols are blocking !!! */

    /* Process the specific first page */
    {
        packet.header.offset = boot_get_word_uart();
        if(packet.header.offset != 0)
        {
            response.code = BOOT_RESP_ERROR_CHUNK_OFFSET;
            response.crc = boot_crc16((const unsigned char*)&(response.code),  sizeof(response.code));
            boot_response_send(response);
            return BOOT_ERROR_TRANSFER;
        }

        packet.header.length = boot_get_subword_uart();
        /* The first packet MUST have the complete atom of data */
        if(packet.header.length != BOOT_SPI_FLASH_ATOM)
        {
            response.code = BOOT_RESP_ERROR_CHUNK_LENGTH;
            response.crc = boot_crc16((const unsigned char*)&(response.code),  sizeof(response.code));
            boot_response_send(response);
            return BOOT_ERROR_SIZE_IMAGE;
        }

        packet.header.crc = boot_get_subword_uart();
        
        status = boot_get_prologue_uart(&imageHeader);
        if(status != BOOT_OK)
        {
            return status;
        }

        /* Set global variables to control the duration of receiving */
        isGettingExe = true;
        gettingExeStartTime = neorv32_clint_time_get();

        /* We get here if and only if image header and chunk header are OK */
        /* Fill the buffer of flash page size in RAM */
        for(; index < packet.header.length / sizeof(uint32_t); index++)
        {
            uint32_t word = boot_get_word_uart();
            packet.buffer[index] = word;
            appChecksum += word;       
        }
        packet.buffer[0] = imageHeader.signature;
        packet.buffer[1] = imageHeader.size;
        packet.buffer[2] = imageHeader.checksum;
        /* Calculate data CRC */
        crc = boot_crc16((const unsigned char*)packet.buffer, packet.header.length);
        if(crc != packet.header.crc)
        {
            response.code = BOOT_RESP_ERROR_CHUNK_CHECKSUM;
            response.crc = boot_crc16((const unsigned char*)&(response.code), sizeof(response.code));
            boot_response_send(response);
            return BOOT_ERROR_CHECKSUM_CHUNK;
        }

        /* Erase the desired sector */
        neo_spi_flash_erase_sector(sectorErasePrev);

        /* In case of first page start not from it's base address to insert a header there later */

        uint32_t* pBufferOffset =  packet.buffer + (sizeof(boot_image_header_t) / sizeof(uint32_t));
        // readSize = index - sizeof(boot_image_header_t) / sizeof(uint32_t);
        readSize = packet.header.length - sizeof(boot_image_header_t);
        neo_spi_flash_write_page(addressSpiData + sizeof(boot_image_header_t), readSize, 
                                    (unsigned char* const)pBufferOffset);
        neo_spi_flash_read_page(addressSpiData + sizeof(boot_image_header_t), readSize, (unsigned char* const)pageRead);
        if(boot_memcmp((const void*)pageRead, pBufferOffset, readSize) != 0)
        {
            response.code = BOOT_RESP_ERROR_PAGE_VERIFY;
            response.crc = boot_crc16((const unsigned char*)&(response.code),  sizeof(response.code));
            boot_response_send(response);
            return BOOT_ERROR_FLASH;
        }
        
        /* The first page has been successfully processed. Send affirmative */
        boot_response_send(response);
        neorv32_gpio_pin_toggle(3);
    }
    
    addressSpiData += BOOT_SPI_FLASH_ATOM;
    /* Now we are awaiting all the firmware data except the first page and image header */
    uint32_t i = BOOT_SPI_FLASH_ATOM - sizeof(boot_image_header_t);
    for(; i < imageHeader.size; i+= BOOT_SPI_FLASH_ATOM, addressSpiData += BOOT_SPI_FLASH_ATOM)
    {
        // size_t delta = (appPrologue.header.size - i);
        // size_t limit = delta > BOOT_SPI_FLASH_ATOM ? BOOT_SPI_FLASH_ATOM / 4 : delta / 4;
        packet.header.offset = boot_get_word_uart();
        if(packet.header.offset <= offsetPrev)
        {
            response.code = BOOT_RESP_ERROR_CHUNK_OFFSET;
            response.crc = boot_crc16((const unsigned char*)&(response.code),  sizeof(response.code));
            boot_response_send(response);
            return BOOT_ERROR_CHUNK_OFFSET;
        }

        packet.header.length = boot_get_subword_uart();
        if(packet.header.length > BOOT_SPI_FLASH_ATOM)
        {
            response.code = BOOT_RESP_ERROR_CHUNK_LENGTH;
            response.crc = boot_crc16((const unsigned char*)&(response.code),  sizeof(response.code));
            boot_response_send(response);
            return BOOT_ERROR_SIZE_IMAGE;
        }

        packet.header.crc = boot_get_subword_uart();
        offsetPrev = packet.header.offset;

        /* Fill the buffer of flash page size in RAM */
        for(index = 0; index < packet.header.length / sizeof(uint32_t); index++)
        {
            uint32_t word = boot_get_word_uart();
            packet.buffer[index] = word;
            appChecksum += word;       
        }

        crc = boot_crc16((const unsigned char*)packet.buffer, packet.header.length);
        if(crc != packet.header.crc)
        {
            response.code = BOOT_RESP_ERROR_CHUNK_CHECKSUM;
            response.crc = boot_crc16((const unsigned char*)&(response.code),  sizeof(response.code));
            boot_response_send(response);
            return BOOT_ERROR_CHECKSUM_CHUNK;
        } 
        
        /* If we're going to write the next sector, we should erase it first */
        if(addressSpiData >= sectorErasePrev + SPI_FLASH_SECTOR_SIZE)
        {
            sectorErasePrev = addressSpiData / SPI_FLASH_SECTOR_SIZE * SPI_FLASH_SECTOR_SIZE;
            neo_spi_flash_erase_sector(sectorErasePrev);
        }

        /* Finally, write the page! */
        neo_spi_flash_write_page(addressSpiData, packet.header.length, (unsigned char* const)(packet.buffer));
        /* Verify page */
        neo_spi_flash_read_page(addressSpiData, packet.header.length, (unsigned char* const)pageRead);
        if(boot_memcmp((const void*)pageRead, (const void*)(packet.buffer), packet.header.length) != 0)
        {
            response.code = BOOT_ERROR_FLASH;
            response.crc = boot_crc16((const unsigned char*)&(response.code), sizeof(response.code));
            boot_response_send(response);
            return BOOT_ERROR_FLASH;
        }
        else
        {
            /* Send affirmative */
            boot_response_send(response);
            neorv32_gpio_pin_toggle(3);
        }
    }

    isGettingExe = false;
    BOOT_PRINTSTR("appChecksum: "); BOOT_PRINTHEX(appChecksum); BOOT_PRINTSTR("\n");
    BOOT_PRINTSTR("appHeader.checksum: "); BOOT_PRINTHEX(imageHeader.checksum); BOOT_PRINTSTR("\n");

    if(appChecksum + imageHeader.checksum != 0)
    {
        response.code = BOOT_RESP_ERROR_IMAGE_CHECKSUM;
        response.crc = boot_crc16((const unsigned char*)&(response.code),  sizeof(response.code));
        boot_response_send(response);
        return BOOT_ERROR_CHECKSUM_IMAGE;
    }
    else
    {
        addressSpiData = addressSpi;
        /* Write the application header in the end in order to get an error during the next boot with invalid image */
        neo_spi_flash_write_word(addressSpiData, imageHeader.signature);
        neo_spi_flash_write_word(addressSpiData + sizeof(imageHeader.signature), imageHeader.size);
        neo_spi_flash_write_word(addressSpiData + sizeof(imageHeader.signature) + sizeof(imageHeader.size), (~appChecksum) + 1);
        /* Set the corresponding indication */
        ledTogglePeriod = NEORV32_SYSINFO->CLK * 2;
        BOOT_PRINTSTR("OK\n");
        BOOT_PRINTSTR("Please, restart the device...\n");
        /* endless sleep mode */
        BOOT_CPU_SLEEP;
    }

    return BOOT_OK;

}

/**********************************************************************//**
* Get executable stream.
**************************************************************************/
static boot_status_t boot_get_prologue_uart(boot_image_header_t* pHeader)
{
    boot_response_t response;
    
    /* image signature (magic bytes) */
    pHeader->signature = boot_get_word_uart();
    if(pHeader->signature != APP_EXE_SIGNATURE)
    {
        response.code = BOOT_RESP_ERROR_IMAGE_SIGN;
        response.crc = boot_crc16((const unsigned char*)&(response.code), sizeof(unsigned char));
        boot_response_send(response);
        return BOOT_ERROR_SIGNATURE;
    }

    /* image size */
    pHeader->size = boot_get_word_uart(); // size in bytes
    if(pHeader->size < 512 || pHeader->size > 4194304)
    {
        response.code = BOOT_RESP_ERROR_IMAGE_LENGTH;
        response.crc = boot_crc16((const unsigned char*)&(response.code), sizeof(unsigned char));
        boot_response_send(response);
        return BOOT_ERROR_SIZE_IMAGE;
    }

    /* image checksum */
    pHeader->checksum = boot_get_word_uart(); // complement sum checksum
    if(pHeader->checksum == 0)
    {
        response.code = BOOT_RESP_ERROR_IMAGE_CHECKSUM;
        response.crc = boot_crc16((const unsigned char*)&(response.code), sizeof(unsigned char));
        boot_response_send(response);
        return BOOT_ERROR_CHECKSUM_IMAGE;
    }

    return BOOT_OK;
}

static boot_status_t boot_get_header_spi(uint32_t exeBase, boot_image_header_t* pHeader)
{
    /* image signature (magic bytes) */
    pHeader->signature = boot_get_word_spi(exeBase + EXE_OFFSET_SIGNATURE);
    BOOT_PRINTSTR("Image sign: "); BOOT_PRINTHEX(pHeader->signature); BOOT_PRINTSTR(" \n");
    if(pHeader->signature != APP_EXE_SIGNATURE)
    {
        return BOOT_ERROR_SIGNATURE;
    }

    /* image size */
    pHeader->size = boot_get_word_spi(exeBase + EXE_OFFSET_SIZE); // size in bytes
    BOOT_PRINTSTR("Image length: "); BOOT_PRINTHEX(pHeader->size); BOOT_PRINTSTR(" \n");
    if(pHeader->size < 512 || pHeader->size > 4194304)
    {
        return BOOT_ERROR_SIZE_IMAGE;
    }

    /* image checksum */
    pHeader->checksum = boot_get_word_spi(exeBase + EXE_OFFSET_CHECKSUM); // complement sum checksum
    BOOT_PRINTSTR("Image cs: "); BOOT_PRINTHEX(pHeader->checksum); BOOT_PRINTSTR(" \n");
    if(pHeader->checksum == 0)
    {
        return BOOT_ERROR_CHECKSUM_IMAGE;
    }

    return BOOT_OK;
}

/**********************************************************************//**
* Get word from executable stream
*
* @param src Source of executable stream data. See #EXE_STREAM_SOURCE_enum.
* @param addr Address when accessing SPI flash / NAND flash (reserved for future).
* @return 32-bit data word from stream.
**************************************************************************/

static void __attribute__((noreturn)) boot_exe_start(uint32_t exeBase)
{
    // deactivate global IRQs
    neorv32_cpu_csr_clr(CSR_MSTATUS, 1 << CSR_MSTATUS_MIE);

    BOOT_PRINTSTR("Booting from ");
    BOOT_PRINTHEX(exeBase);
    BOOT_PRINTSTR("...\n\n");

#if defined(STATUS_LED_EN) && STATUS_LED_EN == 1
    // shut down heart beat LED
    neorv32_gpio_port_set(0);
#endif
    // wait for UART0 to finish transmitting
    while(neorv32_uart0_tx_busy());

    // start application
    asm volatile("jalr ra, %0" : : "r" (exeBase));

    __builtin_unreachable();
    while (1); // should never be reached
}

static inline void boot_system_status(boot_status_t code)
{
    unsigned char status = (unsigned char)code;
    BOOT_PRINTSTR("\a\n"); // output error code with annoying bell sound
    BOOT_PRINTSTR(statusString[status]);
    BOOT_PUTC('\n');
}

/**********************************************************************//**
* Output system error ID and halt.
*
* @param[in] err_code Error code. See #ERROR_CODES and #error_message.
**************************************************************************/
/**
 * @brief 
 * @param err_code 
 */
static void __attribute__((noreturn)) boot_system_error(boot_status_t code)
{
    /* Because the enumeration first member is OK */
    boot_system_status(code);
    
    neorv32_cpu_csr_clr(CSR_MSTATUS, 1 << CSR_MSTATUS_MIE); // deactivate IRQs

    /* permanently light up status LED */
#if defined(STATUS_LED_EN) && STATUS_LED_EN == 1
    if(neorv32_gpio_available())
    {
        neorv32_gpio_port_set(1 << STATUS_LED_PIN);
    }
#endif

    /* endless sleep mode */
    BOOT_CPU_SLEEP;
}
