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

#define EP_DATA_OUT (1 | LIBUSB_ENDPOINT_OUT)
#define EP_CMD_OUT  (2 | LIBUSB_ENDPOINT_OUT)
#define EP_IN       (2 | LIBUSB_ENDPOINT_IN)

#define US_ENGLISH 0x0409



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

static void prepare_command_packet(uint8_t packet[EZP2019_PACKET_SIZE], uint8_t command_id, const Chip *chip)
{
    memset(packet, 0, EZP2019_PACKET_SIZE);
    packet[1] = command_id;
    
    if (chip)
    {
        packet[2] = chip->proto_enum_cfg & 0xFF;
        packet[3] = (chip->proto_enum_cfg >> 8) & 0xFF;

        packet[4] = (chip->pagesize >> 8) & 0xFF;
        packet[5] = chip->pagesize & 0xFF;
        
        packet[6] = (chip->timing_delay >> 8) & 0xFF;
        packet[7] = chip->timing_delay & 0xFF;
        
        packet[8] = (chip->size >> 24) & 0xFF;
        packet[9] = (chip->size >> 16) & 0xFF;
        packet[10] = (chip->size >> 8) & 0xFF;
        packet[11] = chip->size & 0xFF;
        
        packet[12] = (chip->chip_id >> 24) & 0xFF;
        packet[13] = (chip->chip_id >> 16) & 0xFF;
        packet[14] = (chip->chip_id >> 8) & 0xFF;
        packet[15] = chip->chip_id & 0xFF;
        
        packet[28] = (chip->flags >> 24) & 0xFF;
    }
}


static int ezp2019_send_command(libusb_device_handle *handle, const uint8_t command[EZP2019_PACKET_SIZE], uint8_t result[EZP2019_PACKET_SIZE])
{
    int sent = 0;
    int res = 0;
    int received = 0;

    res = libusb_bulk_transfer(handle, EP_CMD_OUT, (unsigned char *)(uintptr_t)command, EZP2019_PACKET_SIZE, &sent, 5000);
    if (res || sent != EZP2019_PACKET_SIZE)
    {
        return EZP2019_LIBUSB_ERROR;
    }

    if (result != NULL)
    {
        usleep(50000);
        res = libusb_bulk_transfer(handle, EP_IN, result, EZP2019_PACKET_SIZE, &received, 5000);
        if (res || received != EZP2019_PACKET_SIZE)
        {
            return EZP2019_LIBUSB_ERROR;
        }
    }

    return 0;
}

static int ezp2019_trigger_transfer(libusb_device_handle *dev, uint32_t address, bool read_mode)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    uint8_t result[EZP2019_PACKET_SIZE] = {0, };
    cmd_packet[1] = 0x05;
    
    cmd_packet[8]  = (address >> 24) & 0xFF;
    cmd_packet[9]  = (address >> 16) & 0xFF;
    cmd_packet[10] = (address >> 8) & 0xFF;
    cmd_packet[11] = address & 0xFF;
    
    return ezp2019_send_command(dev, cmd_packet, read_mode ? result : NULL);
}

static int ezp2019_reset_device(libusb_device_handle *dev)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    cmd_packet[0] = 0x01;
    cmd_packet[1] = 0x08;
    return ezp2019_send_command(dev, cmd_packet, NULL);
}


static int ezp2019_wait_ready(libusb_device_handle *handle, const Chip *chip, const volatile bool *abort)
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

        res = ezp2019_send_command(handle, cmd_packet, result);
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

    if (handle == NULL)
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

int ezp2019_connected_ic(ezp2019 handle, uint32_t *chip_id)
{
    int ret = EZP2019_NO_ERROR;

    struct libusb_device_handle *dev = NULL;
    uint8_t tmp_data[EZP2019_READ_SIZE] = {0, };
    uint8_t data[EZP2019_PACKET_SIZE] = {0, };
    int res = 0;

    if (handle == NULL)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    dev = ezp2019_open_device(handle);
    if (dev)
    {
        libusb_set_auto_detach_kernel_driver(dev, 1);
        res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | 0x01, US_ENGLISH, tmp_data, sizeof(tmp_data), 1000);
        if (res < 0)
        {
            ret = EZP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | 0x02, US_ENGLISH, tmp_data, sizeof(tmp_data), 1000);
        if (res < 0)
        {
            ret = EZP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = libusb_claim_interface(dev, 0);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            goto exit;
        }

        uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
        prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, NULL);

        // Issue Connect command twice: the first time to initialize/wake the SPI lines,
        // and the second time to read the stable JEDEC ID.
        uint8_t dummy_data[EZP2019_PACKET_SIZE] = {0, };
        res = ezp2019_send_command(dev, cmd_packet, dummy_data);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            goto release;
        }

        usleep(50000);

        res = ezp2019_send_command(dev, cmd_packet, data);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            goto release;
        }

        if (chip_id)
        {
            *chip_id = (data[1] << 16) | (data[2] << 8) | data[3];
        }

release:
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            goto exit;
        }

    } else {
        if (errno == EACCES)
        {
            ret = EZP2019_LIBUSB_PERMISSION_DENIED;
        }
        else
        {
            ret = EZP2019_NOT_CONNECTED;
        }
    }

exit:
    if (dev)
    {
        libusb_close(dev);
        dev = NULL;
    }

    return ret;
}

int ezp2019_reset_ic(ezp2019 handle)
{
    int ret = EZP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    int res = 0;

    if (handle == NULL)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    dev = ezp2019_open_device(handle);
    if (!dev)
    {
        ret = (errno == EACCES) ? EZP2019_LIBUSB_PERMISSION_DENIED : EZP2019_NOT_CONNECTED;
        goto exit;
    }
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while claiming interface: %s\n", libusb_strerror(res));
        goto exit;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    prepare_command_packet(cmd_packet, EZP_CMD_RESET, NULL);
    cmd_packet[0] = 0x01;

    res = ezp2019_send_command(dev, cmd_packet, tmp);
    if (res)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while sending reset command: %s\n", libusb_strerror(res));
        goto release;
    }

release:
    res = libusb_release_interface(dev, 0);
    if (res)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        goto exit;
    }

exit:
    if (dev)
    {
        libusb_close(dev);
        dev = NULL;
    }

    return ret;
}

int ezp2019_read_ic(ezp2019 handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort)
{
    int ret = EZP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0)
    {
        return EZP2019_INVALID_ARGUMENT;
    }

    res = ezp2019_connected_ic(handle, &chip_id);
    if (res)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while getting connected IC: %s\n", libusb_strerror(res));
        goto exit;
    }

    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    if (!chip)
    {
        ret = EZP2019_COMMAND_ERROR;
        fprintf(stderr, "Unknown chip ID: 0x%08X\n", chip_id);
        goto exit;
    }

    size_t size = chip->size;

    dev = ezp2019_open_device(handle);
    if (!dev)
    {
        ret = (errno == EACCES) ? EZP2019_LIBUSB_PERMISSION_DENIED : EZP2019_NOT_CONNECTED;
        goto exit;
    }
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while claiming interface: %s\n", libusb_strerror(res));
        goto exit;
    }

    ezp2019_reset_device(dev);
    usleep(100000);

    uint8_t cmd_id = EZP_CMD_READ_SPI;
    if (chip->chip_type && (strcmp(chip->chip_type, "24_EEPROM") == 0 || strcmp(chip->chip_type, "93_EEPROM") == 0))
    {
        cmd_id = EZP_CMD_READ_EE;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };

    prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, chip);
    res = ezp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    prepare_command_packet(cmd_packet, cmd_id, chip);
    res = ezp2019_send_command(dev, cmd_packet, tmp);
    if (res)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while sending read setup command: %s\n", libusb_strerror(res));
        goto release;
    }

    res = ezp2019_trigger_transfer(dev, 0, true);
    if (res)
    {
        ret = EZP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while sending read trigger command: %s\n", libusb_strerror(res));
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

        res = libusb_bulk_transfer(dev, EP_IN, data, (int)transfer_size, &readed, 5000);
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
        res = write(fd, data, bytes_to_write);
        if (res < 0)
        {
            ret = EZP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while writing IC to file descriptor: %s\n", strerror(errno));
            free(data);
            goto release;
        }

        offset += bytes_to_write;
    }
    free(data);

release:
    ezp2019_reset_device(dev);

    if (ret == EZP2019_NO_ERROR)
    {
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        }
    }

exit:
    libusb_close(dev);
    return ret;
}


int ezp2019_write_ic(ezp2019 handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort)
{
    int ret = EZP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0) return EZP2019_INVALID_ARGUMENT;

    res = ezp2019_connected_ic(handle, &chip_id);
    if (res) return EZP2019_LIBUSB_ERROR;

    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    if (!chip) return EZP2019_COMMAND_ERROR;

    size_t size = chip->size;

    dev = ezp2019_open_device(handle);
    if (!dev) return (errno == EACCES) ? EZP2019_LIBUSB_PERMISSION_DENIED : EZP2019_NOT_CONNECTED;
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto exit; }

    ezp2019_reset_device(dev);
    usleep(100000);

    uint8_t cmd_id = EZP_CMD_READ_SPI; 
    if (chip->chip_type && (strcmp(chip->chip_type, "24_EEPROM") == 0 || strcmp(chip->chip_type, "93_EEPROM") == 0))
    {
        cmd_id = EZP_CMD_READ_EE;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };

    prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, chip);
    res = ezp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    prepare_command_packet(cmd_packet, cmd_id, chip);
    res = ezp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    res = ezp2019_trigger_transfer(dev, 0, false); 
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
        res = libusb_bulk_transfer(dev, EP_DATA_OUT, data, (int)chunk_len, &written, 5000);
        if (res) { fprintf(stderr, "libusb_bulk_transfer loop failed at offset %zu with res %d (%s)\n", offset, res, libusb_strerror(res)); ret = EZP2019_LIBUSB_ERROR; free(data); goto release; }
        if (callback) callback(offset + written, size, context);
    }
    free(data);

    if (chip->chip_type && strcmp(chip->chip_type, "SPI_FLASH") == 0)
    {
        usleep(100000);
    }

    res = ezp2019_wait_ready(dev, chip, abort);
    if (res) { fprintf(stderr, "Hardware poll waiting failed at write completion\n"); ret = EZP2019_COMMAND_ERROR; goto release; }

release:
    ezp2019_reset_device(dev);

    if (ret == EZP2019_NO_ERROR)
    {
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        }
    }

exit:
    libusb_close(dev);
    return ret;
}

int ezp2019_verify_ic(ezp2019 handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort, bool *is_matched)
{
    int ret = EZP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0 || is_matched == NULL) return EZP2019_INVALID_ARGUMENT;

    *is_matched = true;

    res = ezp2019_connected_ic(handle, &chip_id);
    if (res) return EZP2019_LIBUSB_ERROR;

    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    if (!chip) return EZP2019_COMMAND_ERROR;

    size_t size = chip->size;

    dev = ezp2019_open_device(handle);
    if (!dev) return (errno == EACCES) ? EZP2019_LIBUSB_PERMISSION_DENIED : EZP2019_NOT_CONNECTED;
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto exit; }

    ezp2019_reset_device(dev);
    usleep(100000);

    uint8_t cmd_id = EZP_CMD_READ_SPI; 
    if (chip->chip_type && (strcmp(chip->chip_type, "24_EEPROM") == 0 || strcmp(chip->chip_type, "93_EEPROM") == 0))
    {
        cmd_id = EZP_CMD_READ_EE;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };

    prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, chip);
    res = ezp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    prepare_command_packet(cmd_packet, cmd_id, chip);
    res = ezp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    res = ezp2019_trigger_transfer(dev, 0, true);
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
        
        res = libusb_bulk_transfer(dev, EP_IN, ic_data, (int)transfer_size, &readed, 5000);
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
    ezp2019_reset_device(dev);

    if (ret == EZP2019_NO_ERROR)
    {
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        }
    }

exit:
    libusb_close(dev);
    return ret;
}

int ezp2019_erase_ic(ezp2019 handle, const volatile bool *abort)
{
    int ret = EZP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint32_t chip_id = 0;
    int res = 0;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    uint8_t tmp2[EZP2019_PACKET_SIZE] = {0, };

    if (handle == NULL) return EZP2019_INVALID_ARGUMENT;

    res = ezp2019_connected_ic(handle, &chip_id);
    if (res) return EZP2019_LIBUSB_ERROR;

    const Chip *chip = ezp2019_find_ic_by_id(chip_id);
    if (!chip) return EZP2019_COMMAND_ERROR;

    dev = ezp2019_open_device(handle);
    if (!dev) return (errno == EACCES) ? EZP2019_LIBUSB_PERMISSION_DENIED : EZP2019_NOT_CONNECTED;
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto exit; }

    ezp2019_reset_device(dev);
    usleep(100000);

    uint8_t cmd_id = EZP_CMD_READ_SPI;
    if (chip->chip_type && (strcmp(chip->chip_type, "24_EEPROM") == 0 || strcmp(chip->chip_type, "93_EEPROM") == 0))
    {
        cmd_id = EZP_CMD_READ_EE;
    }

    uint8_t probe_packet[EZP2019_PACKET_SIZE] = {0, };
    prepare_command_packet(probe_packet, EZP_CMD_CONNECT, chip);
    res = ezp2019_send_command(dev, probe_packet, tmp2);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    uint8_t setup_packet[EZP2019_PACKET_SIZE] = {0, };
    prepare_command_packet(setup_packet, cmd_id, chip);
    res = ezp2019_send_command(dev, setup_packet, tmp);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    uint8_t erase_packet[EZP2019_PACKET_SIZE] = {0, };
    erase_packet[0] = 0x01;
    erase_packet[1] = 0x02;
    erase_packet[0x1A] = 0x80;
    erase_packet[0x1B] = 0x00;
    
    res = ezp2019_send_command(dev, erase_packet, NULL);
    if (res) { ret = EZP2019_LIBUSB_ERROR; goto release; }

    usleep(50000);
    ret = ezp2019_wait_ready(dev, chip, abort);

release:
    ezp2019_reset_device(dev);

    if (ret == EZP2019_NO_ERROR)
    {
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EZP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        }
    }

exit:
    libusb_close(dev);
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
