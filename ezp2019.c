#include "ezp2019.h"

#include <libusb-1.0/libusb.h>

#include <stdbool.h>
#include <errno.h>


#define EZP2019_VID 0x1fc8
#define EZP2019_PID 0x310b

#define EZP2019_PACKET_SIZE 0x40


static unsigned char reset_command[] = {
    0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static_assert(sizeof(reset_command) == EZP2019_PACKET_SIZE, "Illegal reset_command[] size");

static unsigned char connected_ic_command[] = {
    0x00, 0x09, 0x00, 0x00, 0x01, 0x00, 0x03, 0xe8, 0x00, 0x80, 0x00, 0x00, 0x00, 0xef, 0x60, 0x17,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static_assert(sizeof(connected_ic_command) == EZP2019_PACKET_SIZE, "Illegal connected_ic_command[] size");

static int exp2019_command(void *handle, unsigned char command[EZP2019_PACKET_SIZE], unsigned char result[EZP2019_PACKET_SIZE])
{
    int sent = 0;
    int res = 0;
    int recieved = 0;

    res = libusb_bulk_transfer(handle, 0x02, command, EZP2019_PACKET_SIZE, &sent, 5000);
    if (res) return EXP2019_LIBUSB_ERROR;

    res = libusb_bulk_transfer(handle, 0x82, result, EZP2019_PACKET_SIZE, &recieved, 5000);
    if (res) return EXP2019_LIBUSB_ERROR;

    return 0;
}

int exp2019_init(void **handle)
{
    libusb_context *m_ctx = NULL;

    int res = libusb_init(&m_ctx);
    if (res)
    {
        return EXP2019_LIBUSB_ERROR;
    }

    *handle = m_ctx;

    return EXP2019_NO_ERROR;
}

int exp2019_exit(void *handle)
{
    libusb_exit(handle);

    return EXP2019_NO_ERROR;
}

int exp2019_is_connected(void *handle)
{
    libusb_device_handle *dev = NULL;

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (handle)
    {
        libusb_close(dev); dev = NULL;

        return true;
    }

    if (errno == EACCES)
        return true;

    return false;
}

int exp2019_connected_ic(void *handle, unsigned char data[EZP2019_PACKET_SIZE])
{
    int ret = EXP2019_NO_ERROR;

    struct libusb_device_handle *dev = NULL;
    unsigned char tmp_data[256];
    int res = 0;

    dev = libusb_open_device_with_vid_pid(handle, EZP2019_VID, EZP2019_PID);
    if (dev)
    {
        res = libusb_control_transfer(handle, 0x80, 0x06, 0x0301, 0x0409, tmp_data, sizeof(tmp_data), 1000); // (www.zhifengsoft.com)
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = libusb_control_transfer(handle, 0x80, 0x06, 0x0302, 0x0409, tmp_data, sizeof(tmp_data), 1000); // WinUSBComm
        if (res)
        {
            ret = EXP2019_LIBUSB_ERROR;
            goto exit;
        }

        res = libusb_claim_interface(handle, 0);
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
            ret = res;
            goto exit;
        }

        res = libusb_release_interface(handle, 0);
        if (res)
        {
            ret = res;
            goto exit;
        }

    } else {
        ret = EXP2019_NOT_CONNECTED;
    }

exit:
    if (dev)
    {
        libusb_close(dev);
        dev = NULL;
    }

    return ret;
}

int exp2019_read_ic(void *handle, unsigned char *data, volatile bool *abort)
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    // TODO: Not implemented!

    return ret;
}

int exp2019_write_ic(void *handle, unsigned char *data, volatile bool *abort)
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    // TODO: Not implemented!

    return ret;
}

int exp2019_verify_ic(void *handle, unsigned char *data, volatile bool *abort)
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    // TODO: Not implemented!

    return ret;
}


int exp2019_erase_ic(void *handle, volatile bool *abort)
{
    int ret = EXP2019_NOT_IMPLEMENTED;

    // TODO: Not implemented!

    return ret;
}
