#pragma once

#include <stdbool.h>


#ifdef __cplusplus
extern "C"
{
#endif

enum {
    EXP2019_NO_ERROR,
    EXP2019_NOT_CONNECTED,
    EXP2019_LIBUSB_ERROR,
    EXP2019_COMMAND_ERROR,
    EXP2019_NOT_IMPLEMENTED,
};

int exp2019_init(void **handle);
int exp2019_exit(void *handle);
int exp2019_is_connected(void *handle);
int exp2019_connected_ic(void *handle, unsigned char data[]);
int exp2019_read_ic(void *handle, unsigned char *data, volatile bool *abort);
int exp2019_write_ic(void *handle, unsigned char *data, volatile bool *abort);
int exp2019_verify_ic(void *handle, unsigned char *data, volatile bool *abort);
int exp2019_erase_ic(void *handle, volatile bool *abort);

#ifdef __cplusplus
}
#endif
