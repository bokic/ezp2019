#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
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

typedef enum {
    EZP2019_NO_ERROR = 0,
    EZP2019_INVALID_ARGUMENT,
    EZP2019_NOT_CONNECTED,
    EZP2019_LIBUSB_PERMISSION_DENIED,
    EZP2019_LIBUSB_ERROR,
    EZP2019_COMMAND_ERROR,
    EZP2019_ABORTED,
    EZP2019_NOT_IMPLEMENTED,
} ezp2019_error_t;

#define EZP2019_INVALID_CHIP_ID UINT32_MAX

/* Context & handle definitions */
struct libusb_context;
typedef struct libusb_context* ezp2019_handle_t;
typedef ezp2019_handle_t ezp2019;

/* Core Library Functions */
EZP2019_API int ezp2019_init(ezp2019_handle_t *handle);
EZP2019_API int ezp2019_exit(ezp2019_handle_t handle);
EZP2019_API int ezp2019_is_connected(ezp2019_handle_t handle, bool *connected);
EZP2019_API int ezp2019_connected_ic(ezp2019_handle_t handle, uint32_t *chip_id);
EZP2019_API int ezp2019_reset_ic(ezp2019_handle_t handle);
EZP2019_API int ezp2019_read_ic(ezp2019_handle_t handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort_flag);
EZP2019_API int ezp2019_write_ic(ezp2019_handle_t handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort_flag);
EZP2019_API int ezp2019_verify_ic(ezp2019_handle_t handle, int fd, ezp2019_callback_t callback, void *context, const volatile bool *abort_flag, bool *is_matched);
EZP2019_API int ezp2019_erase_ic(ezp2019_handle_t handle, const volatile bool *abort_flag);

EZP2019_API const char *ezp2019_error_string(int error);
EZP2019_API size_t      ezp2019_get_num_chips(void);
EZP2019_API uint32_t    ezp2019_get_ic_id(size_t index);
EZP2019_API const char *ezp2019_get_manufacturer_by_id(uint32_t chip_id);
EZP2019_API const char *ezp2019_get_chip_name_by_id(uint32_t chip_id);
EZP2019_API const char *ezp2019_get_chip_type_by_id(uint32_t chip_id);
EZP2019_API uint32_t    ezp2019_get_chip_size_by_id(uint32_t chip_id);
EZP2019_API uint16_t    ezp2019_get_chip_pagesize_by_id(uint32_t chip_id);
EZP2019_API uint16_t    ezp2019_get_chip_protocol_variant_by_id(uint32_t chip_id);
EZP2019_API uint16_t    ezp2019_get_chip_protocol_enum_cfg_by_id(uint32_t chip_id);
EZP2019_API uint16_t    ezp2019_get_chip_timing_delay_by_id(uint32_t chip_id);
EZP2019_API uint16_t    ezp2019_get_chip_timeout_retries_by_id(uint32_t chip_id);
EZP2019_API uint16_t    ezp2019_get_chip_unknown_delay_by_id(uint32_t chip_id);
EZP2019_API uint32_t    ezp2019_get_chip_flags_by_id(uint32_t chip_id);

#ifdef __cplusplus
}
#endif
