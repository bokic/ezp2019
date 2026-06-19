#include "ezp2019.h"
#include "ezp2019_chips.h"

#include <libusb-1.0/libusb.h>

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>


#define EZP2019_VID 0x1fc8
#define EZP2019_PID 0x310b

#define EZP2019_PACKET_SIZE 0x40

#define EZP2019_READ_SIZE 256

#define EP_DATA_OUT (1 | LIBUSB_ENDPOINT_OUT)
#define EP_CMD_OUT  (2 | LIBUSB_ENDPOINT_OUT)
#define EP_IN       (2 | LIBUSB_ENDPOINT_IN)

#define US_ENGLISH 0x0409

typedef libusb_context *exp2019;



static const Chip *exp2019_find_ic_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
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
        packet[2] = chip->protocol_enum_cfg & 0xFF;
        packet[3] = (chip->protocol_enum_cfg >> 8) & 0xFF;

        packet[4] = (chip->pagesize >> 8) & 0xFF;
        packet[5] = chip->pagesize & 0xFF;
        
        packet[6] = (chip->timeout_retries >> 8) & 0xFF;
        packet[7] = chip->timeout_retries & 0xFF;
        
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


static int exp2019_send_command(void *handle, const uint8_t command[EZP2019_PACKET_SIZE], uint8_t result[EZP2019_PACKET_SIZE])
{
    int sent = 0;
    int res = 0;
    int received = 0;

    res = libusb_bulk_transfer(handle, EP_CMD_OUT, (uint8_t *)command, EZP2019_PACKET_SIZE, &sent, 5000);
    if (res || sent != EZP2019_PACKET_SIZE)
    {
        return EXP2019_LIBUSB_ERROR;
    }

    if (result != NULL)
    {
        usleep(50000);
        res = libusb_bulk_transfer(handle, EP_IN, result, EZP2019_PACKET_SIZE, &received, 5000);
        if (res || received != EZP2019_PACKET_SIZE)
        {
            return EXP2019_LIBUSB_ERROR;
        }
    }

    return 0;
}

static int exp2019_trigger_transfer(libusb_device_handle *dev, uint32_t address, bool read_mode)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    uint8_t result[EZP2019_PACKET_SIZE] = {0, };
    cmd_packet[1] = 0x05;
    
    cmd_packet[8]  = (address >> 24) & 0xFF;
    cmd_packet[9]  = (address >> 16) & 0xFF;
    cmd_packet[10] = (address >> 8) & 0xFF;
    cmd_packet[11] = address & 0xFF;
    
    return exp2019_send_command(dev, cmd_packet, read_mode ? result : NULL);
}
static int exp2019_reset_device(libusb_device_handle *dev)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    cmd_packet[0] = 0x01;
    cmd_packet[1] = 0x08;
    return exp2019_send_command(dev, cmd_packet, NULL);
}


static int exp2019_wait_ready(void *handle, const Chip *chip, volatile bool *abort)
{
    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    int res = 0;
    
    int retries = chip->timeout_retries;

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
            return EXP2019_ABORTED;
        }

        res = exp2019_send_command(handle, cmd_packet, result);
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
    return EXP2019_COMMAND_ERROR;
}

int exp2019_init(exp2019 *handle)
{
    libusb_context *m_ctx = NULL;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    int res = libusb_init(&m_ctx);
    if (res)
    {
        return EXP2019_LIBUSB_ERROR;
    }

    *handle = m_ctx;

    return EXP2019_NO_ERROR;
}

int exp2019_exit(exp2019 handle)
{
    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    libusb_exit(handle);

    return EXP2019_NO_ERROR;
}

int exp2019_is_connected(exp2019 handle, bool *connected)
{
    libusb_device_handle *dev = NULL;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    errno = 0;

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
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

    return EXP2019_NO_ERROR;
}

int exp2019_connected_ic(exp2019 handle, uint32_t *chip_id)
{
    int ret = EXP2019_NO_ERROR;

    struct libusb_device_handle *dev = NULL;
    uint8_t tmp_data[EZP2019_READ_SIZE] = {0, };
    uint8_t data[EZP2019_PACKET_SIZE] = {0, };
    int res = 0;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (dev)
    {
        libusb_set_auto_detach_kernel_driver(dev, 1);
        res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | 0x01, US_ENGLISH, tmp_data, sizeof(tmp_data), 1000);
        if (res < 0)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | 0x02, US_ENGLISH, tmp_data, sizeof(tmp_data), 1000);
        if (res < 0)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = libusb_claim_interface(dev, 0);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
        prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, NULL);

        res = exp2019_send_command(dev, cmd_packet, data);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
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
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

    } else {
        if (errno == EACCES)
        {
            ret = EXP2019_LIBUSB_PERMISSION_DENIED;
        }
        else
        {
            ret = EXP2019_NOT_CONNECTED;
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

int exp2019_reset_ic(exp2019 handle)
{
    int ret = EXP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    int res = 0;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (!dev)
    {
        ret = (errno == EACCES) ? EXP2019_LIBUSB_PERMISSION_DENIED : EXP2019_NOT_CONNECTED;
        goto exit;
    }
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while claiming interface: %s\n", libusb_strerror(res));
        goto exit;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };
    prepare_command_packet(cmd_packet, EZP_CMD_RESET, NULL);
    cmd_packet[0] = 0x01;

    res = exp2019_send_command(dev, cmd_packet, tmp);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while sending reset command: %s\n", libusb_strerror(res));
        goto release;
    }

release:
    res = libusb_release_interface(dev, 0);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
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

int exp2019_read_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort)
{
    int ret = EXP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    res = exp2019_connected_ic(handle, &chip_id);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while getting connected IC: %s\n", libusb_strerror(res));
        goto exit;
    }

    const Chip *chip = exp2019_find_ic_by_id(chip_id);
    if (!chip)
    {
        ret = EXP2019_COMMAND_ERROR;
        fprintf(stderr, "Unknown chip ID: 0x%08X\n", chip_id);
        goto exit;
    }

    size_t size = chip->size;

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (!dev)
    {
        ret = (errno == EACCES) ? EXP2019_LIBUSB_PERMISSION_DENIED : EXP2019_NOT_CONNECTED;
        goto exit;
    }
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while claiming interface: %s\n", libusb_strerror(res));
        goto exit;
    }

    exp2019_reset_device(dev);
    usleep(100000);

    uint8_t cmd_id = EZP_CMD_READ_SPI;
    if (chip->chip_type && (strcmp(chip->chip_type, "24_EEPROM") == 0 || strcmp(chip->chip_type, "93_EEPROM") == 0))
    {
        cmd_id = EZP_CMD_READ_EE;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };

    prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, chip);
    res = exp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    prepare_command_packet(cmd_packet, cmd_id, chip);
    res = exp2019_send_command(dev, cmd_packet, tmp);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while sending read setup command: %s\n", libusb_strerror(res));
        goto release;
    }

    res = exp2019_trigger_transfer(dev, 0, true);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while sending read trigger command: %s\n", libusb_strerror(res));
        goto release;
    }

    size_t pagesize = chip->pagesize ? chip->pagesize : EZP2019_READ_SIZE;
    for (size_t offset = 0; offset < size; offset += pagesize)
    {
        if (abort && *abort)
        {
            ret = EXP2019_ABORTED;
            goto release;
        }

        uint8_t data[256] = {0, };
        int readed = 0;
        size_t chunk = (size - offset < pagesize) ? (size - offset) : pagesize;

        if (callback)
        {
            callback(offset, size, context);
        }

        res = libusb_bulk_transfer(dev, EP_IN, data, (int)chunk, &readed, 5000);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while reading IC: %s\n", libusb_strerror(res));
            goto release;
        }

        res = write(fd, data, (size_t)readed);
        if (res < 0)
        {
            ret = EXP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while writing IC to file descriptor: %s\n", strerror(errno));
            goto release;
        }

        if (readed != (int)chunk)
        {
            ret = EXP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error: Not all bytes read\n");
            goto release;
        }
    }

release:
    exp2019_reset_device(dev);

    if (ret == EXP2019_NO_ERROR)
    {
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        }
    }

exit:
    libusb_close(dev);
    return ret;
}


int exp2019_write_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort)
{
    int ret = EXP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0) return EXP2019_INVALID_ARGUMENT;

    res = exp2019_connected_ic(handle, &chip_id);
    if (res) return EXP2019_LIBUSB_ERROR;

    const Chip *chip = exp2019_find_ic_by_id(chip_id);
    if (!chip) return EXP2019_COMMAND_ERROR;

    size_t size = chip->size;

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (!dev) return (errno == EACCES) ? EXP2019_LIBUSB_PERMISSION_DENIED : EXP2019_NOT_CONNECTED;
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto exit; }

    exp2019_reset_device(dev);
    usleep(100000);

    uint8_t cmd_id = EZP_CMD_READ_SPI; 
    if (chip->chip_type && (strcmp(chip->chip_type, "24_EEPROM") == 0 || strcmp(chip->chip_type, "93_EEPROM") == 0))
    {
        cmd_id = EZP_CMD_READ_EE;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };

    prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, chip);
    res = exp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    prepare_command_packet(cmd_packet, cmd_id, chip);
    res = exp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    res = exp2019_trigger_transfer(dev, 0, false); 
    if (res) { fprintf(stderr, "exp2019_trigger_transfer failed with %d\n", res); ret = EXP2019_LIBUSB_ERROR; goto release; }

    size_t pagesize = chip->pagesize ? chip->pagesize : EZP2019_READ_SIZE;
    for (size_t offset = 0; offset < size; offset += pagesize)
    {
        if (abort && *abort) { ret = EXP2019_ABORTED; goto release; }
        
        uint8_t data[256] = {0, };
        size_t chunk_len = (size - offset < pagesize) ? (size - offset) : pagesize;
        ssize_t n = read(fd, data, chunk_len);
        
        if (n < (ssize_t)chunk_len)
        {
            memset(data + (n > 0 ? n : 0), 0xFF, chunk_len - (n > 0 ? n : 0));
        }
        
        int written = 0;
        res = libusb_bulk_transfer(dev, EP_DATA_OUT, data, (int)chunk_len, &written, 5000);
        if (res) { fprintf(stderr, "libusb_bulk_transfer loop failed at offset %zu with res %d (%s)\n", offset, res, libusb_strerror(res)); ret = EXP2019_LIBUSB_ERROR; goto release; }
        if (callback) callback(offset + written, size, context);
    }

    if (chip->chip_type && strcmp(chip->chip_type, "SPI_FLASH") == 0)
    {
        usleep(100000);
    }

    res = exp2019_wait_ready(dev, chip, abort);
    if (res) { fprintf(stderr, "Hardware poll waiting failed at write completion\n"); ret = EXP2019_COMMAND_ERROR; goto release; }

release:
    exp2019_reset_device(dev);

    if (ret == EXP2019_NO_ERROR)
    {
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        }
    }

exit:
    libusb_close(dev);
    return ret;
}

int exp2019_verify_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort, bool *is_matched)
{
    int ret = EXP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    uint32_t chip_id = 0;
    int res = 0;

    if (handle == NULL || fd < 0 || is_matched == NULL) return EXP2019_INVALID_ARGUMENT;

    *is_matched = true;

    res = exp2019_connected_ic(handle, &chip_id);
    if (res) return EXP2019_LIBUSB_ERROR;

    const Chip *chip = exp2019_find_ic_by_id(chip_id);
    if (!chip) return EXP2019_COMMAND_ERROR;

    size_t size = chip->size;

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (!dev) return (errno == EACCES) ? EXP2019_LIBUSB_PERMISSION_DENIED : EXP2019_NOT_CONNECTED;
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto exit; }

    exp2019_reset_device(dev);
    usleep(100000);

    uint8_t cmd_id = EZP_CMD_READ_SPI; 
    if (chip->chip_type && (strcmp(chip->chip_type, "24_EEPROM") == 0 || strcmp(chip->chip_type, "93_EEPROM") == 0))
    {
        cmd_id = EZP_CMD_READ_EE;
    }

    uint8_t cmd_packet[EZP2019_PACKET_SIZE] = {0, };

    prepare_command_packet(cmd_packet, EZP_CMD_CONNECT, chip);
    res = exp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    prepare_command_packet(cmd_packet, cmd_id, chip);
    res = exp2019_send_command(dev, cmd_packet, tmp);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    res = exp2019_trigger_transfer(dev, 0, true);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    size_t pagesize = chip->pagesize ? chip->pagesize : EZP2019_READ_SIZE;
    for (size_t offset = 0; offset < size; offset += pagesize)
    {
        if (abort && *abort) { ret = EXP2019_ABORTED; goto release; }
        
        uint8_t ic_data[256] = {0, };
        uint8_t file_data[256] = {0, };
        int readed = 0;
        size_t chunk = (size - offset < pagesize) ? (size - offset) : pagesize;
        
        res = libusb_bulk_transfer(dev, EP_IN, ic_data, (int)chunk, &readed, 5000);
        if (res || readed != (int)chunk) { ret = EXP2019_LIBUSB_ERROR; goto release; }
        
        ssize_t n = read(fd, file_data, chunk);
        if (n < (ssize_t)chunk)
        {
            memset(file_data + (n > 0 ? n : 0), 0xFF, chunk - (n > 0 ? n : 0));
        }

        if (memcmp(ic_data, file_data, chunk) != 0)
        {
            *is_matched = false;
        }

        if (callback) callback(offset + readed, size, context);
    }

release:
    exp2019_reset_device(dev);

    if (ret == EXP2019_NO_ERROR)
    {
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        }
    }

exit:
    libusb_close(dev);
    return ret;
}

int exp2019_erase_ic(exp2019 handle, volatile bool *abort)
{
    int ret = EXP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint32_t chip_id = 0;
    int res = 0;
    uint8_t tmp[EZP2019_PACKET_SIZE] = {0, };
    uint8_t tmp2[EZP2019_PACKET_SIZE] = {0, };

    if (handle == NULL) return EXP2019_INVALID_ARGUMENT;

    res = exp2019_connected_ic(handle, &chip_id);
    if (res) return EXP2019_LIBUSB_ERROR;

    const Chip *chip = exp2019_find_ic_by_id(chip_id);
    if (!chip) return EXP2019_COMMAND_ERROR;

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (!dev) return (errno == EACCES) ? EXP2019_LIBUSB_PERMISSION_DENIED : EXP2019_NOT_CONNECTED;
    libusb_set_auto_detach_kernel_driver(dev, 1);

    res = libusb_claim_interface(dev, 0);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto exit; }

    exp2019_reset_device(dev);
    usleep(100000);

    uint8_t cmd_id = EZP_CMD_READ_SPI;
    if (chip->chip_type && (strcmp(chip->chip_type, "24_EEPROM") == 0 || strcmp(chip->chip_type, "93_EEPROM") == 0))
    {
        cmd_id = EZP_CMD_READ_EE;
    }

    uint8_t probe_packet[EZP2019_PACKET_SIZE] = {0, };
    prepare_command_packet(probe_packet, EZP_CMD_CONNECT, chip);
    res = exp2019_send_command(dev, probe_packet, tmp2);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    uint8_t setup_packet[EZP2019_PACKET_SIZE] = {0, };
    prepare_command_packet(setup_packet, cmd_id, chip);
    res = exp2019_send_command(dev, setup_packet, tmp);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    uint8_t erase_packet[EZP2019_PACKET_SIZE] = {0, };
    erase_packet[0] = 0x01;
    erase_packet[1] = 0x02;
    erase_packet[0x1A] = 0x80;
    erase_packet[0x1B] = 0x00;
    
    res = exp2019_send_command(dev, erase_packet, NULL);
    if (res) { ret = EXP2019_LIBUSB_ERROR; goto release; }

    usleep(50000);
    ret = exp2019_wait_ready(dev, chip, abort);

release:
    exp2019_reset_device(dev);

    if (ret == EXP2019_NO_ERROR)
    {
        res = libusb_release_interface(dev, 0);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error while releasing interface: %s\n", libusb_strerror(res));
        }
    }

exit:
    libusb_close(dev);
    return ret;
}

const char *exp2019_error_string(int error)
{
    switch (error)
    {
        case EXP2019_NO_ERROR: return "No error";
        case EXP2019_INVALID_ARGUMENT: return "Invalid argument";
        case EXP2019_NOT_CONNECTED: return "Not connected";
        case EXP2019_LIBUSB_PERMISSION_DENIED: return "Permission denied";
        case EXP2019_LIBUSB_ERROR: return "Libusb error";
        case EXP2019_COMMAND_ERROR: return "Command error";
        case EXP2019_ABORTED: return "Aborted";
        case EXP2019_NOT_IMPLEMENTED: return "Not implemented";
        default: return "Unknown error";
    }
}

size_t exp2019_get_num_chips(void)
{
    return ezp2019_num_chips;
}

uint32_t exp2019_get_ic_id(size_t index)
{
    if (index >= ezp2019_num_chips) return EXP2019_INVALID_CHIP_ID;
    return ezp2019_chips[index].chip_id;
}

const char *exp2019_get_chip_type_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].chip_type;
        }
    }

    return NULL;
}

const char *exp2019_get_manufacturer_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].manufacturer;
        }
    }

    return NULL;
}

const char *exp2019_get_chip_name_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].chip_name;
        }
    }

    return NULL;
}

uint32_t exp2019_get_chip_size_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].size;
        }
    }

    return 0;
}

uint16_t exp2019_get_chip_pagesize_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].pagesize;
        }
    }

    return 0;
}

uint16_t exp2019_get_chip_protocol_enum_cfg_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].protocol_enum_cfg;
        }
    }

    return 0;
}

uint16_t exp2019_get_chip_timeout_retries_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].timeout_retries;
        }
    }

    return 0;
}

uint16_t exp2019_get_chip_unknown_delay_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].unknown_delay;
        }
    }

    return 0;
}

uint32_t exp2019_get_chip_flags_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].flags;
        }
    }

    return 0;
}
