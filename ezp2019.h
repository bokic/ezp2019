#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef ezp2019_EXPORTS
    #define EZP2019_API __declspec(dllexport)
  #else
    #define EZP2019_API __declspec(dllimport)
  #endif
#else
  #if __GNUC__ >= 4
    #define EZP2019_API __attribute__ ((visibility ("default")))
  #else
    #define EZP2019_API
  #endif
#endif

typedef void (*ezp2019_callback_t)(size_t processed, size_t total, void *context);

enum {
    EXP2019_NO_ERROR,
    EXP2019_LIBUSB_PERMISSION_DENIED,
    EXP2019_INVALID_ARGUMENT,
    EXP2019_NOT_CONNECTED,
    EXP2019_LIBUSB_ERROR,
    EXP2019_COMMAND_ERROR,
    EXP2019_ABORTED,
    EXP2019_NOT_IMPLEMENTED,
};

typedef struct libusb_context* exp2019;

EZP2019_API int exp2019_init(exp2019 *handle);
EZP2019_API int exp2019_exit(exp2019 handle);
EZP2019_API int exp2019_is_connected(exp2019 handle, bool *connected);
EZP2019_API int exp2019_connected_ic(exp2019 handle, uint32_t *chip_id);
EZP2019_API int exp2019_reset_ic(exp2019 handle);
EZP2019_API int exp2019_read_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort);
EZP2019_API int exp2019_write_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort);
EZP2019_API int exp2019_verify_ic(exp2019 handle, int fd, ezp2019_callback_t callback, void *context, volatile bool *abort, bool *is_matched);
EZP2019_API int exp2019_erase_ic(exp2019 handle);

EZP2019_API const char *exp2019_error_string(int error);
EZP2019_API size_t      exp2019_get_num_chips(void);
EZP2019_API uint32_t    exp2019_get_ic_id(size_t index);
EZP2019_API const void *exp2019_find_ic_by_id(uint32_t chip_id);
EZP2019_API const char *exp2019_get_manufacturer_by_id(uint32_t chip_id);
EZP2019_API const char *exp2019_get_chip_name_by_id(uint32_t chip_id);
EZP2019_API const char *exp2019_get_chip_type_by_id(uint32_t chip_id);
EZP2019_API uint32_t    exp2019_get_chip_size_by_id(uint32_t chip_id);
EZP2019_API uint16_t    exp2019_get_chip_page_size_by_id(uint32_t chip_id);
EZP2019_API uint16_t    exp2019_get_chip_address_by_id(uint32_t chip_id);
EZP2019_API uint16_t    exp2019_get_chip_timing_by_id(uint32_t chip_id);
EZP2019_API uint16_t    exp2019_get_chip_reserved_by_id(uint32_t chip_id);
EZP2019_API uint32_t    exp2019_get_chip_flags_by_id(uint32_t chip_id);

#ifdef __cplusplus
}
#endif
