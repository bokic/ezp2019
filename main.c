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
    signal(SIGINT, ctrl_c_handler);

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s command\n", argv[0]);
        return EXIT_FAILURE;
    }

    exp2019 handle = NULL;
    int ret = exp2019_init(&handle);
    if (ret)
    {
        fprintf(stderr, "Failed to initialize EZP2019\n");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "is_connected") == 0)
    {
        bool connected;
        ret = exp2019_is_connected(handle, &connected);
        if (ret)
        {
            fprintf(stderr, "Failed to check if connected. Error: %s\n",  exp2019_error_string(ret));
            return EXIT_FAILURE;
        }

        printf("Connected: %s\n", connected ? "true" : "false");
    }
    else if (strcmp(argv[1], "connected_ic") == 0)
    {
        uint32_t chip_id = 0;
        ret = exp2019_connected_ic(handle, &chip_id);
        if (ret)
        {
            fprintf(stderr, "Failed to get connected IC. Error: %s\n",  exp2019_error_string(ret));
            return EXIT_FAILURE;
        }

        const char *manufacturer = exp2019_get_manufacturer_by_id(chip_id);
        const char *chip_name = exp2019_get_chip_name_by_id(chip_id);
        const char *chip_type = exp2019_get_chip_type_by_id(chip_id);

        printf("Chip Info: %s %s (%s)\n", manufacturer, chip_name, chip_type);
    }
    else if (strcmp(argv[1], "read_ic") == 0)
    {
        uint32_t chip_id;
        ret = exp2019_connected_ic(handle, &chip_id);
        if (ret != EXP2019_NO_ERROR)
        {
            fprintf(stderr, "Failed to get connected IC. Error: %s\n",  exp2019_error_string(ret));
            return EXIT_FAILURE;
        }

        uint32_t chip_size = exp2019_get_chip_size_by_id(chip_id);
        uint8_t *data = malloc(chip_size);
        if (!data)
        {
            fprintf(stderr, "Failed to allocate memory for reading IC\n");
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Reading:\n");
        ret = exp2019_read_ic(handle, STDOUT_FILENO, progress_callback, NULL, &abort_flag);
        fprintf(stderr, "\n");
        if (ret != EXP2019_NO_ERROR)
        {
            fprintf(stderr, "Failed to read IC. Error: %s\n",  exp2019_error_string(ret));
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Reading done.\n");
    }
    else if (strcmp(argv[1], "write_ic") == 0)
    {
        char confirm[10];
        fprintf(stderr, "Writing will overwrite the IC data. Are you sure? [y/N]: ");
        if (fgets(confirm, sizeof(confirm), stdin) == NULL || (tolower(confirm[0]) != 'y' && tolower(confirm[0]) != 'Y'))
        {
            fprintf(stderr, "Aborted.\n");
            return EXIT_SUCCESS;
        }

        fprintf(stderr, "Writing:\n");
        ret = exp2019_write_ic(handle, STDIN_FILENO, progress_callback, NULL, &abort_flag);
        fprintf(stderr, "\n");
        if (ret)
        {
            fprintf(stderr, "Failed to write IC. Error: %s\n",  exp2019_error_string(ret));
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Writing done.\n");
    }
    else if (strcmp(argv[1], "verify_ic") == 0)
    {
        uint32_t chip_id;
        ret = exp2019_connected_ic(handle, &chip_id);
        if (ret != EXP2019_NO_ERROR)
        {
            fprintf(stderr, "Failed to get connected IC. Error: %s\n",  exp2019_error_string(ret));
            return EXIT_FAILURE;
        }

        uint32_t chip_size = exp2019_get_chip_size_by_id(chip_id);
        uint8_t *data = malloc(chip_size);
        if (!data)
        {
            fprintf(stderr, "Failed to allocate memory for reading IC\n");
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Verifying:\n");
        ret = exp2019_verify_ic(handle, STDIN_FILENO, progress_callback, NULL, &abort_flag);
        fprintf(stderr, "\n");
        if (ret)
        {
            fprintf(stderr, "Failed to verify IC. Error: %s\n",  exp2019_error_string(ret));
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Verifying done.\n");
    }
    else if (strcmp(argv[1], "erase_ic") == 0)
    {
        char confirm[10];
        fprintf(stderr, "Erasing will erase the entire IC. Are you sure? [y/N]: ");
        if (fgets(confirm, sizeof(confirm), stdin) == NULL || (tolower(confirm[0]) != 'y' && tolower(confirm[0]) != 'Y'))
        {
            fprintf(stderr, "Aborted.\n");
            return EXIT_SUCCESS;
        }

        fprintf(stderr, "Erasing:\n");
        ret = exp2019_erase_ic(handle, &abort_flag);
        fprintf(stderr, "\n");
        if (ret)
        {
            fprintf(stderr, "Failed to erase IC. Error: %s\n",  exp2019_error_string(ret));
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Erasing done.\n");
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    exp2019_exit(handle);

    return EXIT_SUCCESS;
}
