// ================================================================================ //
// SPI flash driver                                                                 //
// ================================================================================ //
#include <neorv32.h>
#include <spi_flash.h>


typedef union
{
    uint32_t uint32;
    uint8_t  uint8[sizeof(uint32_t)];
} u_data_t;


static inline void neo_spi_flash_wait_wip(void);
static void neo_spi_flash_wakeup(void);
static void neo_spi_flash_write_enable(void);
static void neo_spi_flash_write_disable(void);
static uint8_t neo_spi_flash_read_status(void);
static void neo_spi_flash_write_addr(uint32_t addr);


/* Poll status register and when writing is over */
static inline void neo_spi_flash_wait_wip()
{
    uint8_t status;
    do
    {
        // write in progress flag cleared?
        status = neo_spi_flash_read_status();
    }
    while((status & (1 << SPI_FLASH_SREG_BUSY)) != 0);
}

/**********************************************************************//**
 * Wake up flash from deep sleep state
 **************************************************************************/
static void neo_spi_flash_wakeup(void)
{
    neorv32_spi_cs_en(SPI_FLASH_CS);
    neorv32_spi_trans(SPI_FLASH_CMD_WAKE);
    neorv32_spi_cs_dis();
}

/**********************************************************************//**
 * Enable flash write access.
 **************************************************************************/
static void neo_spi_flash_write_enable(void)
{
    neorv32_spi_cs_en(SPI_FLASH_CS);
    neorv32_spi_trans(SPI_FLASH_CMD_WRITE_ENABLE);
    neorv32_spi_cs_dis();
}

/**********************************************************************//**
 * Disable flash write access.
 **************************************************************************/
static void neo_spi_flash_write_disable(void)
{
    neorv32_spi_cs_en(SPI_FLASH_CS);
    neorv32_spi_trans(SPI_FLASH_CMD_WRITE_DISABLE);
    neorv32_spi_cs_dis();
}

/**********************************************************************//**
 * Read flash status register.
 *
 * @return SPI flash status register (32-bit zero-extended).
 **************************************************************************/
static uint8_t neo_spi_flash_read_status(void)
{
    neorv32_spi_cs_en(SPI_FLASH_CS);

    neorv32_spi_trans(SPI_FLASH_CMD_READ_STATUS);
    uint8_t res = neorv32_spi_trans(0);

    neorv32_spi_cs_dis();

    return res;
}

/**********************************************************************//**
 * Send address word to flash (MSB-first, 16-bit, 24-bit or 32-bit address size).
 *
 * @param[in] addr Address word.
 **************************************************************************/
static void neo_spi_flash_write_addr(uint32_t addr)
{
    union
    {
        uint32_t uint32;
        uint8_t  uint8[sizeof(uint32_t)];
    } address;
    

    address.uint32 = addr;

#if defined(SPI_FLASH_ADDR_BYTES) &&  SPI_FLASH_ADDR_BYTES == 2
    neorv32_spi_trans(address.uint8[1]);
    neorv32_spi_trans(address.uint8[0]);
#elif defined(SPI_FLASH_ADDR_BYTES) &&  SPI_FLASH_ADDR_BYTES == 3
    neorv32_spi_trans(address.uint8[2]);
    neorv32_spi_trans(address.uint8[1]);
    neorv32_spi_trans(address.uint8[0]);
#elif defined(SPI_FLASH_ADDR_BYTES) &&  SPI_FLASH_ADDR_BYTES == 4
    neorv32_spi_trans(address.uint8[3]);
    neorv32_spi_trans(address.uint8[2]);
    neorv32_spi_trans(address.uint8[1]);
    neorv32_spi_trans(address.uint8[0]);
#else
    #error "Unsupported SPI_FLASH_ADDR_BYTES configuration!"
#endif
}


/**********************************************************************//**
 * Check if SPI and flash are available/working by making sure the WEL
 * flag of the flash status register can be set and cleared again.
 *
 * @return 0 if success, -1 if error
 **************************************************************************/
int neo_spi_flash_check(void)
{
    // The flash may have been set to sleep prior to reaching this point. Make sure it's alive
    neo_spi_flash_wakeup();

    // set WEL
    neo_spi_flash_write_enable();
    // fail if WEL is cleared
    if((neo_spi_flash_read_status() & (1 << SPI_FLASH_SREG_WEL)) == 0)
    {
        return -1;
    }

    // clear WEL
    neo_spi_flash_write_disable();
    // fail if WEL is set
    if((neo_spi_flash_read_status() & (1 << SPI_FLASH_SREG_WEL)) != 0)
    {
        return -1;
    }

    return 0;
}

/**********************************************************************//**
 * Read byte from SPI flash.
 *
 * @param[in] addr Flash read address.
 * @return Read byte from SPI flash.
 **************************************************************************/
uint8_t neo_spi_flash_read_byte(uint32_t addr)
{
    neorv32_spi_cs_en(SPI_FLASH_CS);    

    neorv32_spi_trans(SPI_FLASH_CMD_READ);
    neo_spi_flash_write_addr(addr);
    uint8_t rdata = neorv32_spi_trans(0);

    neorv32_spi_cs_dis();

    return rdata;
}

void neo_spi_flash_read_page(uint32_t addr, size_t length, uint8_t* const pData)
{
    if(length > SPI_FLASH_PAGE_SIZE)
    {
        length = SPI_FLASH_PAGE_SIZE;
    }

    neorv32_spi_cs_en(SPI_FLASH_CS);

    neorv32_spi_trans(SPI_FLASH_CMD_READ);
    neo_spi_flash_write_addr(addr);

    for(size_t i = 0; i < length; i++)
    {
        pData[i] = neorv32_spi_trans(0);
    }

    neorv32_spi_cs_dis();

    return;
}

/**********************************************************************//**
 * Write byte to SPI flash.
 *
 * @param[in] addr SPI flash read address.
 * @param[in] wdata SPI flash read data.
 **************************************************************************/
void neo_spi_flash_write_byte(uint32_t addr, uint8_t wdata)
{
    neo_spi_flash_write_enable(); // allow write-access

    neorv32_spi_cs_en(SPI_FLASH_CS);

    neorv32_spi_trans(SPI_FLASH_CMD_PAGE_PROGRAM);
    neo_spi_flash_write_addr(addr);
    neorv32_spi_trans(wdata);

    neorv32_spi_cs_dis();
    neo_spi_flash_wait_wip();
}

/**********************************************************************//**
 * Write word to SPI flash.
 *
 * @param addr SPI flash write address.
 * @param wdata SPI flash write data.
 **************************************************************************/
void neo_spi_flash_write_word(uint32_t addr, uint32_t wdata)
{
    u_data_t data;
    data.uint32 = wdata;

    neo_spi_flash_write_enable(); // allow write-access

    neorv32_spi_cs_en(SPI_FLASH_CS);

    neorv32_spi_trans(SPI_FLASH_CMD_PAGE_PROGRAM);
    neo_spi_flash_write_addr(addr);
    for(uint32_t i = 0; i < 4; i++)
    {
        neorv32_spi_trans(data.uint8[i]);
    }
    neorv32_spi_cs_dis();
    neo_spi_flash_wait_wip();

    // little-endian byte order
    // int i;
    // for(i=0; i<4; i++)
    // {
    //     spi_flash_write_byte(addr + i, data.uint8[i]);
    // }
}

void neo_spi_flash_write_page(uint32_t addr, size_t length, uint8_t* const pData)
{
    if(length > SPI_FLASH_PAGE_SIZE)
    {
        length = SPI_FLASH_PAGE_SIZE;
    } 

    neo_spi_flash_write_enable(); // allow write-access

    neorv32_spi_cs_en(SPI_FLASH_CS);

    neorv32_spi_trans(SPI_FLASH_CMD_PAGE_PROGRAM);
    neo_spi_flash_write_addr(addr);

    for(uint32_t i = 0; i < length; i++)
    {
        neorv32_spi_trans(pData[i]);
    }

    neorv32_spi_cs_dis();
    neo_spi_flash_wait_wip();

    return;
}

void neo_spi_flash_write_huge(uint32_t addr, size_t length, uint8_t* const pData)
{
    if(pData == NULL || length == 0)
    {
        return;
    }
    /* Don't write too much */
    if(length > SPI_FLASH_BLOCK_SIZE * 10)
    {
        length = SPI_FLASH_BLOCK_SIZE * 10;
    }

    uint32_t addressErasePrev = addr;
    uint32_t j = 0;
    neo_spi_flash_erase_sector(addressErasePrev);
    while(j < length)
    {      
        if(addr >= addressErasePrev + SPI_FLASH_SECTOR_SIZE)
        {
            neo_spi_flash_erase_sector(addr);
            addressErasePrev = addr;
        }

        neo_spi_flash_write_page(addr, SPI_FLASH_PAGE_SIZE, pData);

        addr += SPI_FLASH_PAGE_SIZE;
        j += SPI_FLASH_PAGE_SIZE;
    }

    return;
}

/**********************************************************************//**
 * Erase sector (4 KiB) at base address.
 *
 * @param[in] addr Base address of sector to erase.
 **************************************************************************/
void neo_spi_flash_erase_sector(uint32_t addr)
{
    neo_spi_flash_write_enable(); // allow write-access

    neorv32_spi_cs_en(SPI_FLASH_CS);

    neorv32_spi_trans(SPI_FLASH_CMD_ERASE_SECTOR);
    neo_spi_flash_write_addr(addr);

    neorv32_spi_cs_dis();

    neo_spi_flash_wait_wip();
}

