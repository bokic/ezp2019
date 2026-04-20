#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C"
{
#endif

enum {
    EXP2019_NO_ERROR,
    EXP2019_LIBUSB_PERMISSION_DENIED,
    EXP2019_INVALID_ARGUMENT,
    EXP2019_NOT_CONNECTED,
    EXP2019_LIBUSB_ERROR,
    EXP2019_COMMAND_ERROR,
    EXP2019_NOT_IMPLEMENTED,
};

#define EZP2019_PACKET_SIZE 0x40

typedef struct libusb_context* exp2019;

int exp2019_init(exp2019 *handle);
int exp2019_exit(exp2019 handle);
int exp2019_is_connected(exp2019 handle, bool *connected);
int exp2019_connected_ic(exp2019 handle, uint8_t data[EZP2019_PACKET_SIZE]);
int exp2019_read_ic(exp2019 handle, uint8_t *data, size_t size, volatile bool *abort);
int exp2019_write_ic(exp2019 handle, uint8_t *data, size_t size, volatile bool *abort);
int exp2019_verify_ic(exp2019 handle, uint8_t *data, size_t size, volatile bool *abort);
int exp2019_erase_ic(exp2019 handle, volatile bool *abort);

const char *exp2019_error_string(int error);

#ifdef __cplusplus
}
#endif
