#include "ezp2019.h"
#include "ezp2019_chips.h"

#include <libusb-1.0/libusb.h>

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>


#define EZP2019_VID 0x1fc8
#define EZP2019_PID 0x310b

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
    if (res) return EXP2019_LIBUSB_ERROR;

    res = libusb_bulk_transfer(handle, EP_IN, result, EZP2019_PACKET_SIZE, &recieved, 5000);
    if (res) return EXP2019_LIBUSB_ERROR;

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

int exp2019_connected_ic(exp2019 handle, uint8_t data[EZP2019_PACKET_SIZE]) // TODO: Should return enum of device ids
{
    int ret = EXP2019_NO_ERROR;

    struct libusb_device_handle *dev = NULL;
    uint8_t tmp_data[256];
    int res = 0;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (dev)
    {
        res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | 0x01, US_ENGLISH, tmp_data, sizeof(tmp_data), 1000); // (www.zhifengsoft.com)
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | 0x02, US_ENGLISH, tmp_data, sizeof(tmp_data), 1000); // WinUSBComm
        if (res)
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

        res = exp2019_command(handle, connected_ic_command, data);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = exp2019_command(handle, reset_command, data);
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

int exp2019_read_ic(exp2019 handle, uint8_t *data, size_t size, volatile bool *abort)
{
    int ret = EXP2019_NO_ERROR;
    libusb_device_handle *dev = NULL;
    uint8_t tmp[EZP2019_PACKET_SIZE];
    int recieved = 0;
    int res = 0;

    if (handle == NULL || data == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
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
        goto exit;
    }

    // Step 1: Send first read trigger command
    res = exp2019_command(dev, read_ic_command_1, tmp);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        goto release;
    }

    // Step 2: Send second read trigger command
    res = exp2019_command(dev, read_ic_command_2, tmp);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        goto release;
    }

    // Step 3: Main data read loop (256 bytes per transfer)
    for (size_t offset = 0; offset < size; offset += 256)
    {
        if (abort && *abort)
        {
            ret = EXP2019_COMMAND_ERROR; // Or a specific SHUTDOWN error
            goto release;
        }

        size_t chunk = (size - offset) > 256 ? 256 : (size - offset);
        res = libusb_bulk_transfer(dev, EP_IN, data + offset, (int)chunk, &recieved, 5000);
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto release;
        }
    }

    // Step 4: Finalize/Reset
    res = exp2019_command(dev, reset_command, tmp);
    if (res)
    {
        ret = EXP2019_LIBUSB_ERROR;
        goto release;
    }

release:
    libusb_release_interface(dev, 0);
exit:
    libusb_close(dev);
    return ret;
}

int exp2019_write_ic(exp2019 handle, uint8_t *data, size_t size, volatile bool *abort) // TODO: Not implemented
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    (void)handle;
    (void)data;
    (void)size;
    (void)abort;

    // TODO: Not implemented!

    return ret;
}

int exp2019_verify_ic(exp2019 handle, uint8_t *data, size_t size, volatile bool *abort) // TODO: Not implemented
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    if (handle == NULL)
    {
        return EXP2019_INVALID_ARGUMENT;
    }

    (void)handle;
    (void)data;
    (void)size;
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
        case EXP2019_NOT_IMPLEMENTED: return "Not implemented";
        default: return "Unknown error";
    }
}
