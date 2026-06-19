#include "ezp2019.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>


static volatile bool abort_flag = false;

static void ctrl_c_handler(int signum)
{
    if (signum == SIGINT)
    {
        abort_flag = true;
    }
}

static void progress_callback(size_t processed, size_t total, void *context)
{
    (void)context;

    size_t percentage = (processed * 100) / total;

    fprintf(stderr, "\rProgress: %lu / %lu (%lu%%)", processed, total, percentage);
    fflush(stderr);
}

int main(int argc, char *argv[])
{
    int ret = EXIT_SUCCESS;

    signal(SIGINT, ctrl_c_handler);

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <command>\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  is_connected  Check if the programmer is connected\n");
        fprintf(stderr, "  connected_ic  Get information about the connected IC\n");
        fprintf(stderr, "  read_ic       Read data from the IC to stdout\n");
        fprintf(stderr, "  write_ic      Write data from stdin to the IC\n");
        fprintf(stderr, "  verify_ic     Verify IC data against stdin\n");
        fprintf(stderr, "  erase_ic      Erase the IC\n");
        return EXIT_FAILURE;
    }

    exp2019 handle = NULL;
    int res = exp2019_init(&handle);
    if (res)
    {
        fprintf(stderr, "Failed to initialize EZP2019\n");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "is_connected") == 0)
    {
        bool connected;
        res = exp2019_is_connected(handle, &connected);
        if (res)
        {
            fprintf(stderr, "Failed to check if connected. Error: %s\n",  exp2019_error_string(res));
            ret = EXIT_FAILURE;
            goto exit;
        }

        printf("Connected: %s\n", connected ? "true" : "false");
    }
    else if (strcmp(argv[1], "connected_ic") == 0)
    {
        uint32_t chip_id = 0;
        res = exp2019_connected_ic(handle, &chip_id);
        if (res)
        {
            fprintf(stderr, "Failed to get connected IC. Error: %s\n",  exp2019_error_string(res));
            ret = EXIT_FAILURE;
            goto exit;
        }

        const char *manufacturer = exp2019_get_manufacturer_by_id(chip_id);
        const char *chip_name = exp2019_get_chip_name_by_id(chip_id);
        const char *chip_type = exp2019_get_chip_type_by_id(chip_id);

        printf("Chip Info: %s %s (%s)\n", manufacturer, chip_name, chip_type);
    }
    else if (strcmp(argv[1], "read_ic") == 0)
    {
        uint32_t chip_id;
        res = exp2019_connected_ic(handle, &chip_id);
        if (res != EXP2019_NO_ERROR)
        {
            fprintf(stderr, "Failed to get connected IC. Error: %s\n",  exp2019_error_string(res));
            ret = EXIT_FAILURE;
            goto exit;
        }

        fprintf(stderr, "Reading:\n");
        ret = exp2019_read_ic(handle, STDOUT_FILENO, progress_callback, NULL, &abort_flag);
        fprintf(stderr, "\n");
        if (ret != EXP2019_NO_ERROR)
        {
            fprintf(stderr, "Failed to read IC. Error: %s\n",  exp2019_error_string(res));
            ret = EXIT_FAILURE;
            goto exit;
        }

        fprintf(stderr, "Reading done.\n");
    }
    else if (strcmp(argv[1], "write_ic") == 0)
    {
        fprintf(stderr, "Writing:\n");
        ret = exp2019_write_ic(handle, STDIN_FILENO, progress_callback, NULL, &abort_flag);
        fprintf(stderr, "\n");
        if (ret)
        {
            fprintf(stderr, "Failed to write IC. Error: %s\n",  exp2019_error_string(res));
            ret = EXIT_FAILURE;
            goto exit;
        }

        fprintf(stderr, "Writing done.\n");
    }
    else if (strcmp(argv[1], "verify_ic") == 0)
    {
        uint32_t chip_id;
        ret = exp2019_connected_ic(handle, &chip_id);
        if (ret != EXP2019_NO_ERROR)
        {
            fprintf(stderr, "Failed to get connected IC. Error: %s\n",  exp2019_error_string(res));
            ret = EXIT_FAILURE;
            goto exit;
        }

        bool is_matched = false;

        fprintf(stderr, "Verifying:\n");
        ret = exp2019_verify_ic(handle, STDIN_FILENO, progress_callback, NULL, &abort_flag, &is_matched);
        fprintf(stderr, "\n");
        if (ret)
        {
            fprintf(stderr, "Failed to verify IC. Error: %s\n",  exp2019_error_string(res));
            ret = EXIT_FAILURE;
            goto exit;
        }

        if (is_matched)
        {
            printf("Data MATCHED!\n");
        }
        else
        {
            printf("Data DID NOT MATCH!\n");
        }
    }
    else if (strcmp(argv[1], "erase_ic") == 0)
    {
        char confirm[10];
        fprintf(stderr, "Erasing will erase the entire IC. Are you sure? [y/N]: ");
        if (fgets(confirm, sizeof(confirm), stdin) == NULL || (tolower(confirm[0]) != 'y' && tolower(confirm[0]) != 'Y'))
        {
            fprintf(stderr, "Aborted.\n");
            goto exit;

        }

        fprintf(stderr, "Erasing:\n");
        ret = exp2019_erase_ic(handle, &abort_flag);
        fprintf(stderr, "\n");
        if (ret)
        {
            fprintf(stderr, "Failed to erase IC. Error: %s\n",  exp2019_error_string(res));
            ret = EXIT_FAILURE;
            goto exit;
        }

        fprintf(stderr, "Erasing done.\n");
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        fprintf(stderr, "Run %s without arguments to see the list of commands.\n", argv[0]);
        ret = EXIT_FAILURE;
        goto exit;
    }

exit:
    exp2019_reset_ic(handle);

    exp2019_exit(handle);

    return ret;
}
