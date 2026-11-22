#include "ezp2019.h"
#include "ezp2019_chips.h"

#include <libusb-1.0/libusb.h>

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


#define EZP2019_VID 0x1fc8
#define EZP2019_PID 0x310b
#define EZP2019_PLUS_PID 0x310c

static const uint16_t ezp2019_pids[] = {
    EZP2019_PID,
    EZP2019_PLUS_PID,
};

/* Open the first connected EZP2019/EZP2019+ device, preserving EACCES on permission errors. */
static libusb_device_handle *ezp2019_open_device(libusb_context *handle)
{
    for (size_t i = 0; i < sizeof(ezp2019_pids) / sizeof(ezp2019_pids[0]); i++)
    {
        errno = 0;
        libusb_device_handle *dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, ezp2019_pids[i]);
        if (dev || errno == EACCES)
        {
            return dev;
        }
    }

    return NULL;
}

#define EZP2019_PACKET_SIZE 0x40
#define EZP2019_READ_SIZE 256

#define EP_WINUSB_IN        (2 | LIBUSB_ENDPOINT_IN)
#define EP_WINUSB_DATA_OUT  (1 | LIBUSB_ENDPOINT_OUT)
#define EP_WINUSB_CMD_OUT   (2 | LIBUSB_ENDPOINT_OUT)

#define EP_WINUSB_DEV_IN        (5 | LIBUSB_ENDPOINT_IN)
#define EP_WINUSB_DEV_DATA_OUT  (5 | LIBUSB_ENDPOINT_OUT)
#define EP_WINUSB_DEV_CMD_OUT   (4 | LIBUSB_ENDPOINT_OUT)

typedef struct {
    libusb_device_handle *handle;
    uint8_t ep_in;
    uint8_t ep_data_out;
    uint8_t ep_cmd_out;
    bool needs_bswap;
} ezp2019_dev_t;

static void ezp2019_detect_endpoints(ezp2019_dev_t *dev)
{
    dev->ep_in = EP_WINUSB_IN;
    dev->ep_data_out = EP_WINUSB_DATA_OUT;
    dev->ep_cmd_out = EP_WINUSB_CMD_OUT;
    dev->needs_bswap = true;

    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(libusb_get_device(dev->handle), &desc) != 0 || desc.iProduct == 0)
    {
        return;
    }

    unsigned char product[256] = {0};
    int len = libusb_get_string_descriptor_ascii(dev->handle, desc.iProduct, product, sizeof(product));
    if (len > 0 && strcmp((const char *)product, "WinUSBComm Device") == 0)
    {
        dev->ep_in = EP_WINUSB_DEV_IN;
        dev->ep_data_out = EP_WINUSB_DEV_DATA_OUT;
        dev->ep_cmd_out = EP_WINUSB_DEV_CMD_OUT;
        dev->needs_bswap = false;
    }
}

static inline void put_u16(uint8_t *dst, uint16_t val, bool be)
{
    if (be)
    {
        dst[0] = (uint8_t)((val >> 8) & 0xFF);
        dst[1] = (uint8_t)(val & 0xFF);
    }
    else
    {
        dst[0] = (uint8_t)(val & 0xFF);
        dst[1] = (uint8_t)((val >> 8) & 0xFF);
    }
}

static inline void put_u32(uint8_t *dst, uint32_t val, bool be)
{
    if (be)
    {
        dst[0] = (uint8_t)((val >> 24) & 0xFF);
        dst[1] = (uint8_t)((val >> 16) & 0xFF);
        dst[2] = (uint8_t)((val >> 8) & 0xFF);
        dst[3] = (uint8_t)(val & 0xFF);
    }
    else
    {
        dst[0] = (uint8_t)(val & 0xFF);
        dst[1] = (uint8_t)((val >> 8) & 0xFF);
        dst[2] = (uint8_t)((val >> 16) & 0xFF);
        dst[3] = (uint8_t)((val >> 24) & 0xFF);
    }
}



static const Chip *ezp2019_find_ic_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < ezp2019_num_chips; i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return &ezp2019_chips[i];
        }
    }

    return NULL;
}

static void prepare_command_packet(uint8_t packet[EZP2019_PACKET_SIZE], uint8_t command_id, const Chip *chip, bool needs_bswap)
{
    memset(packet, 0, EZP2019_PACKET_SIZE);
    packet[1] = command_id;
    
    if (chip)
    {
        packet[2] = chip->proto_enum_cfg & 0xFF;
        packet[3] = (chip->proto_enum_cfg >> 8) & 0xFF;

        put_u16(&packet[4], chip->pagesize, needs_bswap);
        put_u16(&packet[6], chip->timing_delay, needs_bswap);
        put_u32(&packet[8], chip->size, needs_bswap);
        put_u32(&packet[12], chip->chip_id, needs_bswap);

        packet[28] = (chip->flags >> 24) & 0xFF;
    }
}


static int ezp2019_send_command(const ezp2019_dev_t *dev, const uint8_t command[EZP2019_PACKET_SIZE], uint8_t result[EZP2019_PACKET_SIZE])
{
    int sent = 0;
    int res = 0;
    int received = 0;

    res = libusb_bulk_transfer(dev->handle, dev->ep_cmd_out, (unsigned char *)(uintptr_t)command, EZP2019_PACKET_SIZE, &sent, 5000);
    if (res || sent != EZP2019_PACKET_SIZE)
    {
        return EZP2019_LIBUSB_ERROR;
    }

    if (result != NULL)
    {
        usleep(50000);
        res = libusb_bulk_transfer(dev->handle, dev->ep_in, result, EZP2019_PACKET_SIZE, &received, 5000);
        if (res || received != EZP2019_PACKET_SIZE)
        {
            return EZP2019_LIBUSB_ERROR;
        }
    }

    return 0;
}

static int ezp2019_trigger_transfer(const ezp2019_dev_t *dev, uint32_t address, bool read_mode)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    uint8_t result[EZP2019_PACKET_SIZE] = {0, };
    cmd_packet[1] = 0x05;

    put_u32(&cmd_packet[8], address, dev->needs_bswap);

    return ezp2019_send_command(dev, cmd_packet, read_mode ? result : NULL);
}

static int ezp2019_setup_operation(const ezp2019_dev_t *dev, const Chip *chip)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };

    prepare_command_packet(cmd_packet, EZP_CMD_READ_SPI, chip, dev->needs_bswap);
    return ezp2019_send_command(dev, cmd_packet, tmp);
}

static int ezp2019_reset_device(const ezp2019_dev_t *dev)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    cmd_packet[0] = 0x01;
    cmd_packet[1] = 0x08;
    return ezp2019_send_command(dev, cmd_packet, NULL);
}


static int ezp2019_wait_ready(const ezp2019_dev_t *dev, const Chip *chip, const volatile bool *abort)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    int res = 0;
    
    int retries = chip->timing_delay ? chip->timing_delay : 100;

    memset(cmd_packet, 0, EZP2019_PACKET_SIZE);
    cmd_packet[1] = EZP_CMD_STATUS;

    int total_polls = 0;
    while (retries-- > 0)
    {
        total_polls++;
        usleep(50000);
        
        uint8_t result[EZP2019_PACKET_SIZE];
        memset(result, 0xFF, sizeof(result));

        if (abort && *abort)
        {
            return EZP2019_ABORTED;
        }

        res = ezp2019_send_command(dev, cmd_packet, result);
        if (res == 0)
        {
            if ((result[0] & 0x01) == 0)
            {
                if (total_polls > 1) {
                    return 0;
                }
            }
        }
    }
    return EZP2019_COMMAND_ERROR;
}

int ezp2019_init(ezp2019 *handle)
{
    libusb_context *m_ctx = NULL;

    if (handle == NULL)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    int res = libusb_init(&m_ctx);
    if (res)
    {
        return EZP2019_LIBUSB_ERROR;
    }

    *handle = m_ctx;

    return EZP2019_NO_ERROR;
}

int ezp2019_exit(ezp2019 handle)
{
    if (handle == NULL)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    libusb_exit(handle);

    return EZP2019_NO_ERROR;
}

int ezp2019_is_connected(ezp2019 handle, bool *connected)
{
    libusb_device_handle *dev = NULL;

    if (handle == NULL || connected == NULL)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    errno = 0;

    dev = ezp2019_open_device(handle);
    if (dev)
    {
        libusb_close(dev); dev = NULL;

        *connected = true;
    }
    else if (errno == EACCES)
    {
        *connected = true;
    }
    else
    {
        *connected = false;
    }

    return EZP2019_NO_ERROR;
}

static int ezp2019_open_claimed(ezp2019 handle, ezp2019_dev_t *dev)
{
    memset(dev, 0, sizeof(*dev));
    dev->handle = ezp2019_open_device(handle);
    if (!dev->handle)
    {
        return (errno == EACCES) ? EZP2019_LIBUSB_PERMISSION_DENIED : EZP2019_NOT_CONNECTED;
    }

    libusb_set_auto_detach_kernel_driver(dev->handle, 1);
    ezp2019_detect_endpoints(dev);

    int res = libusb_claim_interface(dev->handle, 0);
    if (res)
    {
        fprintf(stderr, "Error while claiming interface: %s\n", libusb_strerror(res));
        libusb_close(dev->handle);
        dev->handle = NULL;
        return EZP2019_LIBUSB_ERROR;
    }

    return EZP2019_NO_ERROR;
}

int ezp2019_connected_ic(ezp2019 handle, uint32_t *chip_id)
{
    int ret = EZP2019_NO_ERROR;
    ezp2019_dev_t dev = {0};
    uint8_t data[EZP2019_PACKET_SIZE] = {0, };
    int res = 0;

    if (handle == NULL)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    ret = ezp2019_open_claimed(handle, &dev);
    if (ret != EZP2019_NO_ERROR)
    {
        return ret;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, NULL, dev.needs_bswap);

    // Issue Connect command twice: the first time to initialize/wake the SPI lines,
    // and the second time to read the stable JEDEC ID.
    uint8_t dummy_data[EZP2019_PACKET_SIZE] = {0, };
    res = ezp2019_send_command(&dev, cmd_packet, dummy_data);
    if (res)
    {
        ret = res;
        goto release;
    }

    usleep(50000);

    res = ezp2019_send_command(&dev, cmd_packet, data);
    if (res)
    {
        ret = res;
        goto release;
    }

    if (chip_id)
    {
        *chip_id = (data[1] << 16) | (data[2] << 8) | data[3];
    }

release:
    res = libusb_release_interface(dev.handle, 0);
    if (res && ret == EZP2019_NO_ERROR)
    {
        ret = EZP2019_LIBUSB_ERROR;
    }

    libusb_close(dev.handle);
    return ret;
}

int ezp2019_reset_ic(ezp2019 handle)
{
    int ret = EZP2019_NO_ERROR;
    ezp2019_dev_t dev = {0};
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    int res = 0;

    if (handle == NULL)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    ret = ezp2019_open_claimed(handle, &dev);
    if (ret != EZP2019_NO_ERROR)
    {
        return ret;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    prepare_command_packet(cmd_packet, EZP_CMD_RESET, NULL, dev.needs_bswap);
    cmd_packet[0] = 0x01;

    res = ezp2019_send_command(&dev, cmd_packet, tmp);
    if (res)
    {
        ret = res;
        fprintf(stderr, "Error while sending reset command: %s\n", ezp2019_error_string(res));
        goto release;
    }

release:
    res = libusb_release_interface(dev.handle, 0);
    if (res && ret == EZP2019_NO_ERROR)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
    }

    libusb_close(dev.handle);
    return ret;
}

int ezp2019_read_ic(ezp2019 handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort)
{
    int ret = EZP2019_NO_ERROR;
    ezp2019_dev_t dev = {0};
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    res = ezp2019_connected_ic(handle, &chip_id);
    if (res)
    {
        fprintf(stderr, "Error while getting connected IC: %s\n", ezp2019_error_string(res));
        return res;
    }

    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    if (!chip)
    {
        fprintf(stderr, "Unknown chip ID: 0x%08X\n", chip_id);
        return EZP2019_COMMAND_ERROR;
    }

    size_t size = chip->size;

    ret = ezp2019_open_claimed(handle, &dev);
    if (ret != EZP2019_NO_ERROR)
    {
        return ret;
    }

    ezp2019_reset_device(&dev);
    usleep(100000);

    res = ezp2019_setup_operation(&dev, chip);
    if (res)
    {
        ret = res;
        fprintf(stderr, "Error while sending read setup command: %s\n", ezp2019_error_string(res));
        goto release;
    }

    res = ezp2019_trigger_transfer(&dev, 0, true);
    if (res)
    {
        ret = res;
        fprintf(stderr, "Error while sending read trigger command: %s\n", ezp2019_error_string(res));
        goto release;
    }

    size_t pagesize = chip->pagesize ? chip->pagesize : EZP2019_READ_SIZE;
    size_t chunk_buf_size = ((pagesize > 4096 ? pagesize : 4096) + 511) & ~(size_t)511;
    uint8_t *data = malloc(chunk_buf_size);
    if (!data) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    for (size_t offset = 0; offset < size; )
    {
        if (abort && *abort)
        {
            ret = EZP2019_ABORTED;
            free(data);
            goto release;
        }

        memset(data, 0, chunk_buf_size);
        int readed = 0;
        size_t remaining = size - offset;
        size_t chunk = (remaining < pagesize) ? remaining : pagesize;
        size_t transfer_size = (chunk < 512) ? 512 : ((chunk + 511) & ~(size_t)511);

        if (callback)
        {
            callback(offset, size, context);
        }

        res = libusb_bulk_transfer(dev.handle, dev.ep_in, data, (int)transfer_size, &readed, 5000);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while reading IC: %s\n", libusb_strerror(res));
            free(data);
            goto release;
        }

        if (readed <= 0)
        {
            ret = EZP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error: Not all bytes read\n");
            free(data);
            goto release;
        }

        size_t bytes_to_write = ((size_t)readed < remaining) ? (size_t)readed : remaining;
        size_t written_total = 0;
        while (written_total < bytes_to_write)
        {
            ssize_t w = write(fd, data + written_total, bytes_to_write - written_total);
            if (w < 0)
            {
                if (errno == EINTR) continue;
                ret = EZP2019_LIBUSB_ERROR;
                fprintf(stderr, "Error while writing IC to file descriptor: %s\n", strerror(errno));
                free(data);
                goto release;
            }
            if (w == 0)
            {
                ret = EZP2019_LIBUSB_ERROR;
                fprintf(stderr, "Error while writing IC to file descriptor: short write\n");
                free(data);
                goto release;
            }
            written_total += (size_t)w;
        }

        offset += bytes_to_write;
    }
    free(data);

release:
    ezp2019_reset_device(&dev);

    res = libusb_release_interface(dev.handle, 0);
    if (res && ret == EZP2019_NO_ERROR)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
    }

    libusb_close(dev.handle);
    return ret;
}


int ezp2019_write_ic(ezp2019 handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort)
{
    int ret = EZP2019_NO_ERROR;
    ezp2019_dev_t dev = {0};
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0) return EZP2019_INVALID_ARGUMENT;

    res = ezp2019_connected_ic(handle, &chip_id);
    if (res) return res;

    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    if (!chip) return EZP2019_COMMAND_ERROR;

    size_t size = chip->size;

    ret = ezp2019_open_claimed(handle, &dev);
    if (ret != EZP2019_NO_ERROR) return ret;

    ezp2019_reset_device(&dev);
    usleep(100000);

    res = ezp2019_setup_operation(&dev, chip);
    if (res) { ret = res; goto release; }

    res = ezp2019_trigger_transfer(&dev, 0, false); 
    if (res) { fprintf(stderr, "ezp2019_trigger_transfer failed with %d\n", res); ret = EZP2019_LIBUSB_ERROR; goto release; }

    size_t pagesize = chip->pagesize ? chip->pagesize : EZP2019_READ_SIZE;
    size_t chunk_buf_size = pagesize > 4096 ? pagesize : 4096;
    uint8_t *data = malloc(chunk_buf_size);
    if (!data) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    for (size_t offset = 0; offset < size; offset += pagesize)
    {
        if (abort && *abort) { ret = EZP2019_ABORTED; free(data); goto release; }
        
        memset(data, 0, chunk_buf_size);
        size_t chunk_len = (size - offset < pagesize) ? (size - offset) : pagesize;
        ssize_t n = read(fd, data, chunk_len);
        
        if (n < (ssize_t)chunk_len)
        {
            memset(data + (n > 0 ? n : 0), 0xFF, chunk_len - (n > 0 ? n : 0));
        }
        
        int written = 0;
        res = libusb_bulk_transfer(dev.handle, dev.ep_data_out, data, (int)chunk_len, &written, 5000);
        if (res) { fprintf(stderr, "libusb_bulk_transfer loop failed at offset %zu with res %d (%s)\n", offset, res, libusb_strerror(res)); ret = EZP2019_LIBUSB_ERROR; free(data); goto release; }
        if (callback) callback(offset + written, size, context);
    }
    free(data);

    if (chip->chip_type && strcmp(chip->chip_type, "SPI_FLASH") == 0)
    {
        usleep(100000);
    }

    res = ezp2019_wait_ready(&dev, chip, abort);
    if (res) { fprintf(stderr, "Hardware poll waiting failed at write completion\n"); ret = EZP2019_COMMAND_ERROR; goto release; }

release:
    ezp2019_reset_device(&dev);

    res = libusb_release_interface(dev.handle, 0);
    if (res && ret == EZP2019_NO_ERROR)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
    }

    libusb_close(dev.handle);
    return ret;
}

int ezp2019_verify_ic(ezp2019 handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort, bool *is_matched)
{
    int ret = EZP2019_NO_ERROR;
    ezp2019_dev_t dev = {0};
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0 || is_matched == NULL) return EZP2019_INVALID_ARGUMENT;

    *is_matched = true;

    res = ezp2019_connected_ic(handle, &chip_id);
    if (res) return res;

    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    if (!chip) return EZP2019_COMMAND_ERROR;

    size_t size = chip->size;

    ret = ezp2019_open_claimed(handle, &dev);
    if (ret != EZP2019_NO_ERROR) return ret;

    ezp2019_reset_device(&dev);
    usleep(100000);

    res = ezp2019_setup_operation(&dev, chip);
    if (res) { ret = res; goto release; }

    res = ezp2019_trigger_transfer(&dev, 0, true);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    size_t pagesize = chip->pagesize ? chip->pagesize : EZP2019_READ_SIZE;
    size_t chunk_buf_size = ((pagesize > 4096 ? pagesize : 4096) + 511) & ~(size_t)511;
    uint8_t *ic_data = malloc(chunk_buf_size);
    uint8_t *file_data = malloc(chunk_buf_size);
    if (!ic_data || !file_data) { free(ic_data); free(file_data); ret = EZP2019_LIBUSB_ERROR; goto release; }

    for (size_t offset = 0; offset < size; )
    {
        if (abort && *abort) { ret = EZP2019_ABORTED; free(ic_data); free(file_data); goto release; }
        
        memset(ic_data, 0, chunk_buf_size);
        memset(file_data, 0, chunk_buf_size);
        int readed = 0;
        size_t remaining = size - offset;
        size_t chunk = (remaining < pagesize) ? remaining : pagesize;
        size_t transfer_size = (chunk < 512) ? 512 : ((chunk + 511) & ~(size_t)511);
        
        res = libusb_bulk_transfer(dev.handle, dev.ep_in, ic_data, (int)transfer_size, &readed, 5000);
        if (res || readed <= 0) { ret = EZP2019_LIBUSB_ERROR; free(ic_data); free(file_data); goto release; }
        
        size_t valid = ((size_t)readed < remaining) ? (size_t)readed : remaining;
        ssize_t n = read(fd, file_data, valid);
        if (n < (ssize_t)valid)
        {
            memset(file_data + (n > 0 ? n : 0), 0xFF, valid - (n > 0 ? n : 0));
        }

        if (memcmp(ic_data, file_data, valid) != 0)
        {
            *is_matched = false;
        }

        offset += valid;
        if (callback) callback(offset, size, context);
    }
    free(ic_data);
    free(file_data);

release:
    ezp2019_reset_device(&dev);

    res = libusb_release_interface(dev.handle, 0);
    if (res && ret == EZP2019_NO_ERROR)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
    }

    libusb_close(dev.handle);
    return ret;
}

int ezp2019_erase_ic(ezp2019 handle, const volatile bool *abort)
{
    int ret = EZP2019_NO_ERROR;
    ezp2019_dev_t dev = {0};
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL) return EZP2019_INVALID_ARGUMENT;

    res = ezp2019_connected_ic(handle, &chip_id);
    if (res) return res;

    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    if (!chip) return EZP2019_COMMAND_ERROR;

    ret = ezp2019_open_claimed(handle, &dev);
    if (ret != EZP2019_NO_ERROR) return ret;

    ezp2019_reset_device(&dev);
    usleep(100000);

    res = ezp2019_setup_operation(&dev, chip);
    if (res) { ret = res; goto release; }

    uint8_t erase_packet[EZP2019_PACKET_SIZE] = {0, };
    erase_packet[0] = 0x01;
    erase_packet[1] = 0x02;
    put_u16(&erase_packet[0x1A], 0x8000, dev.needs_bswap);

    res = ezp2019_send_command(&dev, erase_packet, NULL);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    usleep(50000);
    ret = ezp2019_wait_ready(&dev, chip, abort);

release:
    ezp2019_reset_device(&dev);

    res = libusb_release_interface(dev.handle, 0);
    if (res && ret == EZP2019_NO_ERROR)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
    }

    libusb_close(dev.handle);
    return ret;
}

const char *ezp2019_error_string(int error)
{
    switch (error)
    {
        case EZP2019_NO_ERROR: return "No error";
        case EZP2019_INVALID_ARGUMENT: return "Invalid argument";
        case EZP2019_NOT_CONNECTED: return "Not connected";
        case EZP2019_LIBUSB_PERMISSION_DENIED: return "Permission denied";
        case EZP2019_LIBUSB_ERROR: return "Libusb error";
        case EZP2019_COMMAND_ERROR: return "Command error";
        case EZP2019_ABORTED: return "Aborted";
        case EZP2019_NOT_IMPLEMENTED: return "Not implemented";
        default: return "Unknown error";
    }
}

size_t ezp2019_get_num_chips(void)
{
    return ezp2019_num_chips;
}

uint32_t ezp2019_get_ic_id(size_t index)
{
    if (index >= ezp2019_num_chips) return EZP2019_INVALID_CHIP_ID;
    return ezp2019_chips[index].chip_id;
}

const char *ezp2019_get_chip_type_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->chip_type : NULL;
}

const char *ezp2019_get_manufacturer_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->manufacturer : NULL;
}

const char *ezp2019_get_chip_name_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->chip_name : NULL;
}

uint32_t ezp2019_get_chip_size_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->size : 0;
}

uint16_t ezp2019_get_chip_pagesize_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->pagesize : 0;
}

uint16_t ezp2019_get_chip_protocol_enum_cfg_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->proto_enum_cfg : 0;
}

uint16_t ezp2019_get_chip_protocol_variant_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->protocol_variant : 0;
}

uint16_t ezp2019_get_chip_timing_delay_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->timing_delay : 0;
}

uint16_t ezp2019_get_chip_timeout_retries_by_id(uint32_t chip_id)
{
    return ezp2019_get_chip_timing_delay_by_id(chip_id);
}

uint16_t ezp2019_get_chip_unknown_delay_by_id(uint32_t chip_id)
{
    return ezp2019_get_chip_protocol_variant_by_id(chip_id);
}

uint32_t ezp2019_get_chip_flags_by_id(uint32_t chip_id)
{
    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    return chip ? chip->flags : 0;
}
