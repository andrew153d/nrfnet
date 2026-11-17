/*
 *
 * Copyright (c) 2024, Copyright (c) 2024 TMRh20 & 2bndy5
 * All rights reserved.
 *
 *
 */
#include "linux/gpio.h"
#include <unistd.h>    // close()
#include <fcntl.h>     // open()
#include <sys/ioctl.h> // ioctl()
#include <errno.h>     // errno, strerror()
#include <string.h>    // std::string, strcpy()
#include <map>
#include "gpio.h"

// instantiate some global structs to setup cache
// doing this globally ensures the data struct is zero-ed out
typedef int gpio_fd; // for readability
std::map<rf24_gpio_pin_t, gpio_fd> cachedPins;
struct gpio_v2_line_request request;
struct gpio_v2_line_values data;

// initialize static member.
int GPIOChipCache::fd = -1;

void GPIOChipCache::openDevice()
{
    if (fd < 0) {
        fd = open(RF24_LINUX_GPIO_CHIP, O_RDONLY);
        if (fd < 0) {
            std::string msg = "Can't open device ";
            msg += RF24_LINUX_GPIO_CHIP;
            msg += "; ";
            msg += strerror(errno);
            throw GPIOException(msg);
            return;
        }
    }
}

void GPIOChipCache::closeDevice()
{
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

GPIOChipCache::GPIOChipCache()
{
    request.num_lines = 1;
    strcpy(request.consumer, "RF24 lib");
    data.mask = 1ULL; // only change value for specified pin
}

GPIOChipCache::~GPIOChipCache()
{
    closeDevice();
    for (std::map<rf24_gpio_pin_t, gpio_fd>::iterator i = cachedPins.begin(); i != cachedPins.end(); ++i) {
        if (i->second > 0) {
            close(i->second);
        }
    }
}

// GPIO chip cache manager
GPIOChipCache gpioCache;

GPIO::GPIO()
{
}

GPIO::~GPIO()
{
}

void GPIO::open(rf24_gpio_pin_t port, int DDR)
{
    // Export the GPIO pin
    FILE *export_file = fopen("/sys/class/gpio/export", "w");
    if (export_file == NULL) {
        throw GPIOException("Failed to open GPIO export file");
        return;
    }
    fprintf(export_file, "%d", port);
    fclose(export_file);

    // Set the direction of the GPIO pin
    char direction_path[50];
    snprintf(direction_path, sizeof(direction_path), "/sys/class/gpio/gpio%d/direction", port);
    FILE *direction_file = fopen(direction_path, "w");
    if (direction_file == NULL) {
        throw GPIOException("Failed to open GPIO direction file");
        return;
    }
    fprintf(direction_file, DDR ? "out" : "in");
    fclose(direction_file);
}

void GPIO::close(rf24_gpio_pin_t port)
{
    // Unexport the GPIO pin
    FILE *unexport_file = fopen("/sys/class/gpio/unexport", "w");
    if (unexport_file == NULL) {
        throw GPIOException("Failed to open GPIO unexport file");
        return;
    }
    fprintf(unexport_file, "%d", port);
    fclose(unexport_file);
}

int GPIO::read(rf24_gpio_pin_t port)
{
    // Read the value of the GPIO pin
    char value_path[50];
    snprintf(value_path, sizeof(value_path), "/sys/class/gpio/gpio%d/value", port);
    FILE *value_file = fopen(value_path, "r");
    if (value_file == NULL) {
        throw GPIOException("Failed to open GPIO value file");
        return -1;
    }

    char value;
    if (fread(&value, 1, 1, value_file) != 1) {
        fclose(value_file);
        throw GPIOException("Failed to read GPIO value");
        return -1;
    }
    fclose(value_file);

    return value == '1' ? 1 : 0;
}

void GPIO::write(rf24_gpio_pin_t port, int value)
{
    // Write the value to the GPIO pin
    char value_path[50];
    snprintf(value_path, sizeof(value_path), "/sys/class/gpio/gpio%d/value", port);
    FILE *value_file = fopen(value_path, "w");
    if (value_file == NULL) {
        throw GPIOException("Failed to open GPIO value file");
        return;
    }
    fprintf(value_file, "%d", value);
    fclose(value_file);
}
