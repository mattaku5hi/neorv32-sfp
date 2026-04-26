#include <chrono>
#include <cstring>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <ftd2xx.h>


#ifdef APP_EXE_SIGNATURE
#define LOADER_MSG_SIGNATURE            APP_EXE_SIGNATURE
#else
#define LOADER_MSG_SIGNATURE            0xb0bacafe
#endif

#ifdef APP_CHUNK_SIZE                   
#define LOADER_MSG_CHUNK_SIZE           APP_CHUNK_SIZE
#else
#define LOADER_MSG_CHUNK_SIZE           256
#endif

#ifndef LOADER_HEXDUMP_LINELENGTH
#define LOADER_HEXDUMP_LINELENGTH       16
#endif

#ifdef APP_CHUNK_CRC_INITIAL            
#define LOADER_MSG_CHUNK_CRC_INITIAL    APP_CHUNK_CRC_INITIAL
#else
#define LOADER_MSG_CHUNK_CRC_INITIAL    0xffff
#endif

#define LOADER_UART_BAUDRATE            B115200

#ifndef LOADER_MAX_RETRY_AMOUNT
#define LOADER_MAX_RETRY_AMOUNT         3
#endif

#ifndef LOADER_RESPONSE_TIMEOUT_S       
#define LOADER_RESPONSE_TIMEOUT_S       5
#endif

#ifndef LOADER_DEVICES_MAX
#define LOADER_DEVICES_MAX              4
#endif

#define LOADER_KIBI                     1024
#define LOADER_MEBI                     1024 * LOADER_KIBI


/**
 * @brief 
 */
class ImageLoader
{
    public:
        ImageLoader(const std::string& fileImagePath);
        ~ImageLoader();
        /* Prevent instance copy and copy with assignment */
        ImageLoader(const ImageLoader&) = delete;
        ImageLoader& operator=(const ImageLoader&) = delete;

        enum class loaderStatusCode;
        struct loaderStatusTuple;

        uint16_t calcCrc16(const uint8_t* pData, size_t length, const uint16_t initial = LOADER_MSG_CHUNK_CRC_INITIAL);
        void hexDump(const void* pData, size_t length, size_t lengthLine = LOADER_HEXDUMP_LINELENGTH);
        loaderStatusTuple uartFind(DWORD& locId);
        loaderStatusTuple uartOpen(const DWORD locId);
        loaderStatusTuple uartSetup(const void* pHandle);
        loaderStatusTuple dataSend(FT_HANDLE pHandle, LPVOID pData, DWORD length);
        loaderStatusTuple responseAwait(FT_HANDLE pHandle, DWORD length, const DWORD timeOutS = LOADER_RESPONSE_TIMEOUT_S);
        loaderStatusTuple imageProcess();

        enum class loaderStatusCode
        {
            LOADER_OK,

            LOADER_ERROR_ARGS,
            LOADER_ERROR_OUTOFMEM,
            LOADER_ERROR_FILE_ACCESS,
            LOADER_ERROR_SERIAL_OPEN,
            LOADER_ERROR_SERIAL_SETUP,
            LOADER_ERROR_FILE_OP_WRITE,
            LOADER_ERROR_IMAGE_INVALID,

            LOADER_ERROR_MSG_SIGN,
            LOADER_ERROR_MSG_TIMEOUT,
            LOADER_ERROR_MSG_LENGTH_IMAGE,
            LOADER_ERROR_MSG_LENGTH_CHUNK,
            LOADER_ERROR_MSG_CHECKSUM_CHUNK,
            LOADER_ERROR_MSG_CHECKSUM_IMAGE,
            LOADER_ERROR_MSG_CHUNK_OFFSET,
            LOADER_ERROR_PAGE_VERIFY,

            LOADER_ERROR_MSG_RESPONSE_LENGTH,
            
            LOADER_ERROR_UNKNOWN,
        };
        struct loaderStatusTuple
        {
            loaderStatusCode code;
            std::string desc;
        };

        struct handleDeleter
        {
            void operator()(FT_HANDLE handle) const
            {
                if(handle != nullptr && handle != reinterpret_cast<FT_HANDLE>(INVALID_HANDLE_VALUE))
                {
                    FT_Close(handle);
                }
            }
        };

    
    private:
        enum class loaderResponseStatusCode : uint16_t;
        struct loaderPrologue;
        struct loaderMsg;
        struct loaderMsgHeader;
        struct  __attribute__((packed)) loaderResponse;
        std::string m_file_image;
        using imageLoaderHandle = std::unique_ptr<std::remove_pointer<HANDLE>::type, handleDeleter>;
        imageLoaderHandle m_file_device_handle;

        inline loaderStatusTuple response2status(loaderResponseStatusCode responseCode);
        loaderStatusTuple imageCheck(std::ifstream& fileImage);
        DWORD uartChoose(FT_DEVICE_LIST_INFO_NODE* pDevicesInfo, const DWORD devicesAmount);
        loaderStatusTuple responseProcess(FT_HANDLE pHandle, LPVOID pData, DWORD length);
        loaderStatusTuple packetProcess(LPVOID pData, DWORD length, 
                                        const DWORD retriesMax = LOADER_MAX_RETRY_AMOUNT,
                                        const DWORD timeOutS = LOADER_RESPONSE_TIMEOUT_S);

        /* Min custom type size is 4 bytes for RISC-V platform (it doesn't support struct packing) */
        enum class loaderResponseStatusCode : uint16_t
        {
            LOADER_MSG_OK,

            LOADER_ERROR_MSG_SIGN,
            LOADER_ERROR_MSG_TIMEOUT,
            LOADER_ERROR_MSG_LENGTH_IMAGE,
            LOADER_ERROR_MSG_LENGTH_CHUNK,
            LOADER_ERROR_MSG_CHECKSUM_CHUNK,
            LOADER_ERROR_MSG_CHECKSUM_IMAGE,
            LOADER_ERROR_MSG_PAGE_VERIFY,
            
            LOADER_ERROR_MSG_UNKNOWN,
        };
        struct loaderPrologue
        {
            uint32_t signature;
            uint32_t length;
            uint32_t checksum;
        };
        struct loaderMsgHeader
        {
            uint32_t offset;
            uint16_t length;
            uint16_t crc;
        };
        struct loaderMsg
        {
            loaderMsgHeader header;
            uint8_t data[LOADER_MSG_CHUNK_SIZE];
        };
        struct __attribute__((packed)) loaderResponse
        {
            loaderResponseStatusCode code;
            uint16_t crc;
        };
};


/**
 * @brief 
 * @param fileImagePath 
 * @param fileDevicePath 
 */
ImageLoader::ImageLoader(const std::string& fileImagePath) :
    m_file_image(fileImagePath), m_file_device_handle(nullptr)
{
    if(m_file_image.empty())
    {
        throw std::runtime_error("Failed to acquire resource: empty string");
    }

} 

/**
 * @brief 
 */
ImageLoader::~ImageLoader()
{
}


inline ImageLoader::loaderStatusTuple ImageLoader::response2status(ImageLoader::loaderResponseStatusCode responseCode)
{
    std::string sPrefix = "Device response: ";
    switch(responseCode)
    {
        case loaderResponseStatusCode::LOADER_MSG_OK:
            return {loaderStatusCode::LOADER_OK, sPrefix + "Data packet has been delivered successfully"};
        case loaderResponseStatusCode::LOADER_ERROR_MSG_SIGN:
            return {loaderStatusCode::LOADER_ERROR_MSG_SIGN, sPrefix + "Incorrect image signature"};
        case loaderResponseStatusCode::LOADER_ERROR_MSG_LENGTH_IMAGE:
            return {loaderStatusCode::LOADER_ERROR_MSG_LENGTH_IMAGE, sPrefix + "Incorrect image size"};
        case loaderResponseStatusCode::LOADER_ERROR_MSG_LENGTH_CHUNK:
            return {loaderStatusCode::LOADER_ERROR_MSG_LENGTH_CHUNK, sPrefix + "Incorrect chunk size"};
        case loaderResponseStatusCode::LOADER_ERROR_MSG_CHECKSUM_CHUNK:
            return {loaderStatusCode::LOADER_ERROR_MSG_CHECKSUM_CHUNK, sPrefix + "Incorrect data packet checksum"};
        case loaderResponseStatusCode::LOADER_ERROR_MSG_CHECKSUM_IMAGE:
            return {loaderStatusCode::LOADER_ERROR_MSG_CHECKSUM_IMAGE, sPrefix + "Incorrect image checksum"};
        case loaderResponseStatusCode::LOADER_ERROR_MSG_PAGE_VERIFY:
            return {loaderStatusCode::LOADER_ERROR_PAGE_VERIFY, sPrefix + "SPI flash page verification has failed"};
        case loaderResponseStatusCode::LOADER_ERROR_MSG_UNKNOWN:
        default:
            return {loaderStatusCode::LOADER_ERROR_UNKNOWN, sPrefix + "Unknown transfer error code has been received"};
    }
}


uint16_t ImageLoader::calcCrc16(const uint8_t* pData, size_t length, const uint16_t initial) 
{
    uint16_t crc = initial; // Initial value
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

DWORD ImageLoader::uartChoose(FT_DEVICE_LIST_INFO_NODE* pDevicesInfo, const DWORD devicesAmount)
{
    if(pDevicesInfo == nullptr)
    {
        return 0;
    }

    /* The default LocId belongs to the last device */
    DWORD targetLocId = pDevicesInfo[devicesAmount - 1].LocId;

    for(DWORD i = 0; i < devicesAmount - 1; i++)
    {
        /* Compare the nearby structs in order to re-define the LocId var */
        if(pDevicesInfo[i].ID == pDevicesInfo[i + 1].ID &&
            pDevicesInfo[i].Flags == pDevicesInfo[i + 1].Flags &&
            pDevicesInfo[i].Type == pDevicesInfo[i + 1].Type &&
            pDevicesInfo[i + 1].LocId == pDevicesInfo[i].LocId + 1 &&
            std::string(pDevicesInfo[i].SerialNumber).substr(0, std::string(pDevicesInfo[i].SerialNumber).size() - 1) == 
                std::string(pDevicesInfo[i + 1].SerialNumber).substr(0, std::string(pDevicesInfo[i + 1].SerialNumber).size() - 1) &&
            std::string(pDevicesInfo[i].Description).substr(0, std::string(pDevicesInfo[i].Description).size() - 1) == 
                std::string(pDevicesInfo[i + 1].Description).substr(0, std::string(pDevicesInfo[i + 1].Description).size() - 1) &&
            pDevicesInfo[i].SerialNumber[std::string(pDevicesInfo[i].SerialNumber).size() - 1] == 'A' &&
            pDevicesInfo[i + 1].SerialNumber[std::string(pDevicesInfo[i + 1].SerialNumber).size() - 1] == 'B' &&
            pDevicesInfo[i].Description[std::string(pDevicesInfo[i].Description).size() - 1] == 'A' &&
            pDevicesInfo[i + 1].Description[std::string(pDevicesInfo[i + 1].Description).size() - 1] == 'B')
        {
                /* Found a matching pair, use the LocId of the 'B' device */
            targetLocId = pDevicesInfo[i + 1].LocId;
        }
    }

    return targetLocId;
}

ImageLoader::loaderStatusTuple ImageLoader::uartFind(DWORD& locId)
{
    FT_STATUS status;
    DWORD deviceAmount{0};
    size_t deviceAmountActual{0};
    FT_DEVICE_LIST_INFO_NODE* deviceInfoArray{nullptr};

    // status = FT_ListDevices(reinterpret_cast<PVOID>(pDevicesBuffer), reinterpret_cast<PVOID>(&deviceAmount), 
    //                                 FT_LIST_ALL | FT_OPEN_BY_SERIAL_NUMBER);
    status = FT_CreateDeviceInfoList(&deviceAmount);
    if(status != FT_OK)
    {
        std::string desc = "Failed to enumerate devices. Error: " + std::to_string(status);
        return {loaderStatusCode::LOADER_ERROR_SERIAL_OPEN, desc};
    }
    else if(deviceAmount == 0)
    {
        return {loaderStatusCode::LOADER_ERROR_SERIAL_OPEN, "No FTDI devices have been found"};
    }
    
    deviceAmountActual = deviceAmount > LOADER_DEVICES_MAX ? LOADER_DEVICES_MAX : deviceAmount;
    deviceInfoArray = new FT_DEVICE_LIST_INFO_NODE[deviceAmountActual];
    if(deviceInfoArray == nullptr)
    {
        return {loaderStatusCode::LOADER_ERROR_OUTOFMEM, "Memory allocation for devices info buffer has failed"};
    }

    status = FT_GetDeviceInfoList(deviceInfoArray, &deviceAmount);
    if(status != FT_OK)
    {
        return {loaderStatusCode::LOADER_ERROR_SERIAL_OPEN, "Failed to get serial devices list"};
    }

    locId = this->uartChoose(deviceInfoArray, deviceAmount);

    delete[] deviceInfoArray;
    deviceInfoArray = nullptr;

    return {loaderStatusCode::LOADER_OK, "The FTDI device has been detected"};
}

ImageLoader::loaderStatusTuple ImageLoader::uartOpen(const DWORD locId)
{
    FT_HANDLE handle;
    FT_STATUS status;
    DWORD driverVersion;
    DWORD id = locId;
    std::string statusDesc;
    
    if(locId == 0)
    {
        statusDesc = "Incorrect params have been given for func ";
        statusDesc += __FUNCTION__;
        return {loaderStatusCode::LOADER_ERROR_ARGS, statusDesc};
    }

    std::cout << "Openning the device (port) with LocId " << id << '\n';
    // status = FT_Open(port, &handle);
    status = FT_OpenEx(reinterpret_cast<PVOID>(id), FT_OPEN_BY_LOCATION, &handle);
    if(status != FT_OK)
    {
        statusDesc = "Failed to open device (port) with LocId ";
        statusDesc += std::to_string(id);
        statusDesc += ". Error: " + std::to_string(status);
        return {loaderStatusCode::LOADER_ERROR_SERIAL_OPEN, statusDesc};
    }

    status = FT_GetDriverVersion(handle, &driverVersion);
    if(status != FT_OK)
    {
        statusDesc = "Failed to get device (port) with LocId ";
        statusDesc += std::to_string(id);
        statusDesc += " driver version. Error: " + std::to_string(status);
        return {loaderStatusCode::LOADER_ERROR_SERIAL_OPEN, statusDesc};
    }
    else
    {
        std::cout << "Device driver version: 0x" << std::setw(8) << std::setfill('0') 
            << std::hex << driverVersion << '\n';
        m_file_device_handle.reset(handle);
    }


    statusDesc = "FTDI device " + std::to_string(id) + "has been opened";
    return {loaderStatusCode::LOADER_OK, statusDesc};
}

ImageLoader::loaderStatusTuple ImageLoader::uartSetup(const void* pHandle)
{
    FT_STATUS status;
    std::string statusDesc;

    if(pHandle == nullptr || pHandle == reinterpret_cast<const void*>(INVALID_HANDLE_VALUE))
    {
        statusDesc = "Incorrect params have been given for func ";
        statusDesc += __FUNCTION__;
        return {loaderStatusCode::LOADER_ERROR_ARGS, statusDesc};
    }
    const FT_HANDLE pFtHandle = const_cast<FT_HANDLE>(pHandle);

    status = FT_ResetDevice(pFtHandle);
    if(status != FT_OK)
    {
        return {loaderStatusCode::LOADER_ERROR_SERIAL_SETUP, "Failed to reset the device"};
    }

    status = FT_SetFlowControl(pFtHandle, FT_FLOW_NONE, 0, 0);
    if(status != FT_OK)
    {
        return {loaderStatusCode::LOADER_ERROR_SERIAL_SETUP, "Failed to set the device flow control"};
    }

    status = FT_SetDataCharacteristics(pFtHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
    if(status != FT_OK)
    {
        return {loaderStatusCode::LOADER_ERROR_SERIAL_SETUP, "Failed to set the device data characteristics"};
    }

    status = FT_SetBaudRate(pFtHandle, FT_BAUD_115200);
    if(status != FT_OK)
    {
        return {loaderStatusCode::LOADER_ERROR_SERIAL_SETUP, "Failed to set the device baud rate"};
    }
   
    return {loaderStatusCode::LOADER_OK, "FTDI device has been set up"};
}

ImageLoader::loaderStatusTuple ImageLoader::dataSend(FT_HANDLE pHandle, LPVOID pData, DWORD length)
{
    FT_STATUS status;
    std::string statusDesc;
    DWORD bytesWritten;

    if(pHandle == nullptr || pHandle == reinterpret_cast<FT_HANDLE>(INVALID_HANDLE_VALUE) || pData == nullptr || length == 0)
    {
        statusDesc = "Incorrect params have been given for func ";
        statusDesc += __FUNCTION__;
        return {loaderStatusCode::LOADER_ERROR_ARGS, statusDesc};
    }
    
    status = FT_Write(pHandle, pData, length, &bytesWritten);
    if(status != FT_OK)
    {
        return {loaderStatusCode::LOADER_ERROR_FILE_OP_WRITE, "Failed to start writing to the device"};
    }
    else if(bytesWritten != length)
    {
        return {loaderStatusCode::LOADER_ERROR_FILE_OP_WRITE, "Failed to write all the bytes to the device"};
    }
    else
    {
        return {loaderStatusCode::LOADER_OK, "All the data has been written to device"};
    }
}

ImageLoader::loaderStatusTuple ImageLoader::responseAwait(FT_HANDLE pHandle, DWORD length, const DWORD timeOutS)
{
    std::string statusDesc;
    if(pHandle == nullptr || pHandle == reinterpret_cast<FT_HANDLE>(INVALID_HANDLE_VALUE) || length == 0 || timeOutS == 0)
    {
        statusDesc = "Incorrect params have been given for func ";
        statusDesc += __FUNCTION__;
        return {loaderStatusCode::LOADER_ERROR_ARGS, statusDesc};
    }

    FT_STATUS status;
    DWORD bytesReceived{0};
    unsigned int queueChecks{0};
    auto timeStart = std::chrono::high_resolution_clock::now();

    while(bytesReceived < length)
    {
        queueChecks++;
        /* Check for timeout periodically */
        if(queueChecks % 32 == 0)
        {
            auto timeCurrent = std::chrono::high_resolution_clock::now();
            auto timeElapsed = std::chrono::duration_cast<std::chrono::seconds>(timeCurrent - timeStart).count();
            if(timeElapsed >= timeOutS)
            {
                return {loaderStatusCode::LOADER_ERROR_MSG_TIMEOUT, "The receive timeout has occured"};
            }
        }
        
        status = FT_GetQueueStatus(pHandle, &bytesReceived);
        if(status != FT_OK)
        {
            return {loaderStatusCode::LOADER_ERROR_FILE_ACCESS, "Failed to get device queue status"};
        }
    }

    statusDesc = "Successfully received " + std::to_string(bytesReceived) + " bytes\n";
    return {loaderStatusCode::LOADER_OK, statusDesc};   
}

ImageLoader::loaderStatusTuple ImageLoader::responseProcess(FT_HANDLE pHandle, LPVOID pResponse, DWORD length)
{
    FT_STATUS status;
    DWORD bytesRead{0};
    std::string statusDesc;
    loaderStatusTuple responseStatus;
    uint16_t crc{0};

    if(pHandle == nullptr || pHandle == reinterpret_cast<FT_HANDLE>(INVALID_HANDLE_VALUE) || length == 0)
    {
        statusDesc = "Incorrect params have been given for func ";
        statusDesc += __FUNCTION__;
        return {loaderStatusCode::LOADER_ERROR_ARGS, statusDesc};
    }    
        
    status = FT_Read(pHandle, pResponse, length, &bytesRead);
    if(status != FT_OK)
    {
        return {loaderStatusCode::LOADER_ERROR_FILE_ACCESS, "Failed to read from device"};
    }

    /* Print response */
    std::cout << "-----RESPONSE-----" << std::endl;
    this->hexDump(pResponse, length);

    /* Check the response itself */
    /* Incorrect response has been received so try again */
    if(bytesRead != length)
    {
        return {loaderStatusCode::LOADER_ERROR_MSG_RESPONSE_LENGTH, "Response of invalid size has been received"};
    }
    crc = this->calcCrc16(reinterpret_cast<const uint8_t*>(pResponse), sizeof(loaderResponseStatusCode));
    /* Incorrect response CRC has been received so try again */
    if(crc != static_cast<loaderResponse*>(pResponse)->crc)
    {
        return {loaderStatusCode::LOADER_ERROR_MSG_CHECKSUM_CHUNK, "The response with incorrect checksum has been received"};
    }

    responseStatus = this->response2status(static_cast<loaderResponse*>(pResponse)->code);
    return responseStatus;

}

ImageLoader::loaderStatusTuple ImageLoader::packetProcess(LPVOID pData, DWORD length, 
                                                            const DWORD retriesMax, const DWORD timeOutS)
{
    std::string statusDesc;
    if(pData == nullptr || length == 0 || retriesMax == 0 || timeOutS == 0)
    {
        statusDesc = "Incorrect params have been given for func ";
        statusDesc += __FUNCTION__;
        return {loaderStatusCode::LOADER_ERROR_ARGS, statusDesc};
    }

    FT_HANDLE pHandle = m_file_device_handle.get();
    loaderStatusTuple status;
    loaderResponse response{};

    for(unsigned attempt = 0; attempt <= retriesMax; attempt++)
    {
        status = this->dataSend(pHandle, pData, length);
        if(status.code != loaderStatusCode::LOADER_OK)
        {
            /* Retry */
            continue;
        }

        status = this->responseAwait(pHandle, static_cast<DWORD>(sizeof(response)));
        if(status.code != loaderStatusCode::LOADER_OK)
        {
            /* Retry */
            continue;
        }
        
        status = this->responseProcess(pHandle, static_cast<LPVOID>(&response), static_cast<DWORD>(sizeof(response)));
        if(status.code != loaderStatusCode::LOADER_OK)
        {
            /* Retry */
            continue;
        }
        else
        {
            /* Response is positive */
            return {loaderStatusCode::LOADER_OK, "Data packet has been processed correctly"};
        }
    }
    
    return {loaderStatusCode::LOADER_ERROR_MSG_TIMEOUT, "Max retries reached without success"};
}

ImageLoader::loaderStatusTuple ImageLoader::imageCheck(std::ifstream& fileImage)
{
    size_t imageSize = 0;
    const uint32_t imageSignature = LOADER_MSG_SIGNATURE;
    loaderStatusTuple status;
    size_t bytesCounter = 0;
    loaderPrologue prologue;
    size_t imageHeaderSize = sizeof(prologue);

    if(!fileImage.is_open())
    {
        return {loaderStatusCode::LOADER_ERROR_FILE_ACCESS, "Cannot access the provided binary file"};
    }
    
    fileImage.seekg(0, std::ios::end);
    imageSize = fileImage.tellg();
    fileImage.seekg(0, std::ios::beg);

    /* Set up the specific first packet */
    fileImage.read(reinterpret_cast<char*>(&prologue), sizeof(prologue));
    bytesCounter += fileImage.gcount();
    if(bytesCounter != sizeof(prologue))
    {
        return {loaderStatusCode::LOADER_ERROR_FILE_ACCESS, "Failed to read image header"};
    }
    
    std::cout<<"Image magic word: "<<prologue.signature<<std::endl;
    std::cout<<"Image length: "<<prologue.length<<std::endl;
    std::cout<<"Image checksum: "<<prologue.checksum<<std::endl;

    if(prologue.signature != imageSignature)
    {
        return {loaderStatusCode::LOADER_ERROR_IMAGE_INVALID, "Incorrect image has been specified"};
    }
    if(prologue.length < LOADER_KIBI || prologue.length >= 4 * LOADER_MEBI || prologue.length != (imageSize - imageHeaderSize))
    {
        return {loaderStatusCode::LOADER_ERROR_IMAGE_INVALID, "Incorrect image has been specified"};
    }

    return {loaderStatusCode::LOADER_OK, "Image is valid"};
}

void ImageLoader::hexDump(const void* pData, size_t length, size_t lengthLine)
{   
    if(pData == nullptr || length == 0)
    {
        return;
    }

    const uint8_t* pDataByte = static_cast<const uint8_t*>(pData);
    for(size_t i = 0; i < length; i += lengthLine)
    {
        /* Print offset in hex */
        // std::print("{:08x} : ", i);
        std::cout << std::setw(8) << std::setfill('0') << std::hex << i << " : "; 
        /* Print aligned hex values */
        for(size_t j = 0; j < lengthLine; j++)
        {
            if(i + j < length)
            {
                /* Print the hex value itself */
                // std::print("{:02x} ", static_cast<int>(pDataByte[i + j]));
                std::cout << std::setw(2) << std::setfill('0') << static_cast<int>(pDataByte[i + j]) << " ";
            }
            /* Perform alignment */
            else
            {
                /* Print spaces */
                // std::print("   ");
                std::cout << "   ";
            }
        }

        /* Print aligned represented hex values in ASCII */
        // std::print(" | ");
        std::cout << " | ";
        for(size_t j = 0; j < lengthLine; j++)
        {
            if(i + j < length)
            {
                char c = pDataByte[i + j];
                if(c >= 32 && c <= 126)
                {
                    // std::print("{}", c);
                    std::cout << c;
                } 
                else
                {
                    // std::print(".");
                    std::cout << '.';
                }
            }
            else
            {
                // std::print(" ");
                std::cout << ' ';
            }
        }

        // std::println();
        std::cout << std::endl;
    }

    return;
}


ImageLoader::loaderStatusTuple ImageLoader::imageProcess()
{
    size_t imageSize = 0;
    size_t bytesCounter = 0;
    std::streamsize bytesRead = 0;
    uint32_t imageOffset = 0;
    ImageLoader::loaderMsg packet;
    loaderStatusTuple status;
    // std::wstring fileDevice;
    DWORD portNumber = 0;

    /* Open the image file */
    /* RAII good: it will be cleaned up automatically */
    std::ifstream fileImage(m_file_image, std::ios::binary);
    status = this->imageCheck(fileImage);
    if(status.code != loaderStatusCode::LOADER_OK)
    {
        return status;
    }
    fileImage.seekg(0, std::ios::beg);
    
    /* Look for the first COM device available */
    status = ImageLoader::uartFind(portNumber);
    if(status.code != loaderStatusCode::LOADER_OK)
    {
        return status;
    }

    /* Open the serial UART device */
    status = ImageLoader::uartOpen(portNumber);
    if(status.code != loaderStatusCode::LOADER_OK)
    {
        return status;
    }

    /* Set up the serial UART device */
    status = ImageLoader::uartSetup(const_cast<LPVOID>(m_file_device_handle.get()));
    if(status.code != loaderStatusCode::LOADER_OK)
    {
        return status;
    }

    while(true)
    {
        fileImage.read(reinterpret_cast<char*>(packet.data), sizeof(packet.data));
        bytesRead = fileImage.gcount();
        if(bytesRead == 0)
        {
            break;
        }
        bytesCounter += bytesRead;
       
        packet.header.offset = imageOffset;
        packet.header.length = static_cast<uint16_t>(bytesRead);
        packet.header.crc = this->calcCrc16(packet.data, bytesRead);
        std::cout << "-----ATOM_" << std::dec << 1 + imageOffset / LOADER_MSG_CHUNK_SIZE << "-----" << std::endl;
        this->hexDump(reinterpret_cast<const void*>(&packet), sizeof(packet.header) + static_cast<size_t>(bytesRead));

        status = this->packetProcess(reinterpret_cast<LPVOID>(&packet), sizeof(packet.header) + bytesRead);
        if(status.code != loaderStatusCode::LOADER_OK)
        {
            return status;
        }
        
        imageOffset += bytesRead;
        imageSize += bytesRead;
    }

    /* Explicit cleanup */
    fileImage.close();
    return {loaderStatusCode::LOADER_OK, "Image file has been transferred successfully"};
}


int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_image>" << std::endl;
        return static_cast<int>(ImageLoader::loaderStatusCode::LOADER_ERROR_ARGS);
    }

    const std::string fileImagePath(argv[1]);
    ImageLoader loader(fileImagePath);
    
    ImageLoader::loaderStatusTuple status = loader.imageProcess();
    if(status.code != ImageLoader::loaderStatusCode::LOADER_OK)
    {
        std::cerr << "Error: " << status.desc << std::endl;
    }
    else
    {
        std::cout << status.desc << std::endl;
    }

    return static_cast<int>(status.code);
}