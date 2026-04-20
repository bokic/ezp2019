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

#define EP_OUT (2 | LIBUSB_ENDPOINT_OUT)
#define EP_IN  (2 | LIBUSB_ENDPOINT_IN)

#define US_ENGLISH 0x0409

typedef libusb_context *exp2019;

static uint8_t reset_command[EZP2019_PACKET_SIZE] = {
    0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static_assert(sizeof(reset_command) == EZP2019_PACKET_SIZE, "Illegal reset_command[] size");

static uint8_t connected_ic_command[EZP2019_PACKET_SIZE] = {
    0x00, 0x09, 0x00, 0x00, 0x01, 0x00, 0x03, 0xe8, 0x00, 0x80, 0x00, 0x00, 0x00, 0xef, 0x60, 0x17,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static_assert(sizeof(connected_ic_command) == EZP2019_PACKET_SIZE, "Illegal connected_ic_command[] size");

static uint8_t read_ic_command_1[EZP2019_PACKET_SIZE] = {
    0x00, 0x07, 0x00, 0x00, 0x01, 0x00, 0x03, 0xe8, 0x00, 0x01, 0x00, 0x00, 0x00, 0x68, 0x40, 0x10,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static_assert(sizeof(read_ic_command_1) == EZP2019_PACKET_SIZE, "Illegal read_ic_command_1[] size");

static uint8_t read_ic_command_2[EZP2019_PACKET_SIZE] = {
    0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static_assert(sizeof(read_ic_command_2) == EZP2019_PACKET_SIZE, "Illegal read_ic_command_2[] size");

static int exp2019_command(void *handle, uint8_t command[EZP2019_PACKET_SIZE], uint8_t result[EZP2019_PACKET_SIZE])
{
    int sent = 0;
    int res = 0;
    int recieved = 0;

    res = libusb_bulk_transfer(handle, EP_OUT, command, EZP2019_PACKET_SIZE, &sent, 5000);
    if (res)
    {
        return EXP2019_LIBUSB_ERROR;
    }

    res = libusb_bulk_transfer(handle, EP_IN, result, EZP2019_PACKET_SIZE, &recieved, 5000);
    if (res)
    {
        return EXP2019_LIBUSB_ERROR;
    }

    return 0;
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
    uint8_t data[EZP2019_PACKET_SIZE];
    int res = 0;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (dev)
    {
        libusb_set_auto_detach_kernel_driver(dev, 1);
        res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | 0x01, US_ENGLISH, tmp_data, sizeof(tmp_data), 1000); // (www.zhifengsoft.com)
        if (res < 0)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | 0x02, US_ENGLISH, tmp_data, sizeof(tmp_data), 1000); // WinUSBComm
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

        res = exp2019_command(dev, connected_ic_command, data);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        if (chip_id)
        {
            *chip_id = (data[1] << 16) | (data[2] << 8) | data[3];
        }

        res = exp2019_command(dev, reset_command, data);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

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

int exp2019_read_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort)
{
    int ret = EXP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE];
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
        goto release;
    }

    size_t size = exp2019_get_chip_size_by_id(chip_id);
    if (size == 0)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while getting chip size: %s\n", libusb_strerror(res));
        goto release;
    }

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (dev)
    {
        libusb_set_auto_detach_kernel_driver(dev, 1);
    }

    if (!dev)
    {
        if (errno == EACCES)
        {
            ret = EXP2019_LIBUSB_PERMISSION_DENIED;
        }
        else
        {
            ret = EXP2019_NOT_CONNECTED;
        }

        goto exit;
    }

    res = libusb_claim_interface(dev, 0);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while claiming interface: %s\n", libusb_strerror(res));
        goto exit;
    }

    // Step 1: Send first read trigger command
    res = exp2019_command(dev, read_ic_command_1, tmp);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while sending read trigger command: %s\n", libusb_strerror(res));
        goto release;
    }

    // Step 2: Send second read trigger command
    res = exp2019_command(dev, read_ic_command_2, tmp);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while sending second read trigger command: %s\n", libusb_strerror(res));
        goto release;
    }

    // Step 3: Main data read loop (EZP2019_READ_SIZE bytes per transfer)
    for (size_t offset = 0; offset < size; offset += EZP2019_READ_SIZE)
    {
        if (abort && *abort)
        {
            ret = EXP2019_ABORTED;
            goto release;
        }

        uint8_t data[EZP2019_READ_SIZE];
        int readed = 0;
        size_t chunk = EZP2019_READ_SIZE;

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

        if (readed != EZP2019_READ_SIZE)
        {
            ret = EXP2019_LIBUSB_ERROR;
            fprintf(stderr, "Error: Not all bytes read\n");
            goto release;
        }
    }

    // Step 4: Finalize/Reset
    res = exp2019_command(dev, reset_command, tmp);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        fprintf(stderr, "Error while resetting IC: %s\n", libusb_strerror(res));
        goto release;
    }

release:
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

int exp2019_write_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort) // TODO: Not implemented
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    (void)handle;
    (void)fd;
    (void)callback;
    (void)context;
    (void)abort;

    // TODO: Not implemented!

    return ret;
}

int exp2019_verify_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort) // TODO: Not implemented
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    (void)handle;
    (void)fd;
    (void)callback;
    (void)context;
    (void)abort;

    // TODO: Not implemented!

    return ret;
}


int exp2019_erase_ic(exp2019 handle, volatile bool *abort) // TODO: Not implemented
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    (void)handle;
    (void)abort;

    // TODO: Not implemented!

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

const void *exp2019_find_ic_by_id(uint32_t chip_id)
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

uint16_t exp2019_get_chip_page_size_by_id(uint32_t chip_id)
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

uint16_t exp2019_get_chip_address_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].address;
        }
    }

    return 0;
}

uint16_t exp2019_get_chip_timing_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].timing;
        }
    }

    return 0;
}

uint16_t exp2019_get_chip_reserved_by_id(uint32_t chip_id)
{
    for (size_t i = 0; i < sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]); i++)
    {
        if (ezp2019_chips[i].chip_id == chip_id)
        {
            return ezp2019_chips[i].reserved;
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
