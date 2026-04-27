// ================================================================================ //
// SPI flash driver                                                                 //
// ================================================================================ //

#ifndef SPI_FLASH_H
#define SPI_FLASH_H


#include <stdint.h>


/* -------- SPI configuration -------- */

/** Enable SPI (default) including SPI flash boot options */
#ifndef SPI_EN
#define SPI_EN  1
#endif

/** SPI flash chip select (low-active) at SPI.spi_csn_o(SPI_FLASH_CS) */
#ifndef SPI_FLASH_CS
#define SPI_FLASH_CS    0
#endif

/** SPI flash address width (in numbers of bytes; 2,3,4) */
#ifndef SPI_FLASH_ADDR_BYTES
#define SPI_FLASH_ADDR_BYTES    3 // default = 3 address bytes = 24-bit
#endif

/** SPI flash sector size in bytes */
#ifndef SPI_FLASH_SECTOR_SIZE
#define SPI_FLASH_SECTOR_SIZE   4096 // default = 64kB
#endif

/** SPI flash block (16 sectors) size in bytes */
#ifndef SPI_FLASH_BLOCK_SIZE
#define SPI_FLASH_BLOCK_SIZE   65536 // default = 64kB
#endif

#ifndef SPI_FLASH_PAGE_SIZE
#define SPI_FLASH_PAGE_SIZE     256
#endif // SPI_FLASH_PAGE_SIZE


int neo_spi_flash_check(void);
uint8_t neo_spi_flash_read_byte(uint32_t addr);
void neo_spi_flash_read_page(uint32_t addr, size_t length, uint8_t* const pData);
void neo_spi_flash_write_byte(uint32_t addr, uint8_t wdata);
void neo_spi_flash_write_word(uint32_t addr, uint32_t wdata);
void neo_spi_flash_write_page(uint32_t addr, size_t length, uint8_t* const pData);
void neo_spi_flash_write_huge(uint32_t addr, size_t length, uint8_t* const pData);
void neo_spi_flash_erase_sector(uint32_t addr);


/**********************************************************************//**
* SPI flash commands
**************************************************************************/
typedef enum  
{
    SPI_FLASH_CMD_PAGE_PROGRAM      = 0x02, /**< Program page */
    SPI_FLASH_CMD_READ              = 0x03, /**< Read data */
    SPI_FLASH_CMD_WRITE_DISABLE     = 0x04, /**< Disallow write access */
    SPI_FLASH_CMD_READ_STATUS       = 0x05, /**< Get status register */
    SPI_FLASH_CMD_WRITE_ENABLE      = 0x06, /**< Allow write access */
    SPI_FLASH_CMD_WAKE              = 0xAB, /**< Wake up from sleep mode */
    SPI_FLASH_CMD_ERASE_SECTOR      = 0x20,
    SPI_FLASH_CMD_ERASE_BLOCK_32K   = 0x52,  /**< Erase complete block of 32 KiB */
    SPI_FLASH_CMD_ERASE_BLOCK_64K   = 0xd8,  /**< Erase complete block of 32 KiB */
 } spi_flash_cmd_t;


/**********************************************************************//**
* SPI flash status register bits
**************************************************************************/
typedef enum
{
    SPI_FLASH_SREG_BUSY = 0, /**< Busy, write/erase in progress when set, read-only */
    SPI_FLASH_SREG_WEL  = 1  /**< Write access enabled when set, read-only */
} spi_flash_reg_status_t;



#endif  // SPI_FLASH_H