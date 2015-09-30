/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdlib.h>

/* Zephyr includes */
#include "generated/autoconf.h"
#include "atomic.h"
#include "device.h"
#include "gpio.h"

#define SOL_LOG_DOMAIN &_log_domain
#include "sol-gpio.h"
#include "sol-log-internal.h"
#include "sol-mainloop.h"
#include "sol-util.h"
#include "sol-vector.h"

SOL_LOG_INTERNAL_DECLARE_STATIC(_log_domain, "gpio");

#define RING_BUFFER_SIZE 256

/* Since there's no way to predict how many pins a board have using Zephyr,
 * we'll use this ring buffer in order to schedule the pending gpio interruptions */
struct ring_buffer {
    uint32_t data[RING_BUFFER_SIZE];
    atomic_val_t start;
    atomic_val_t end;
};

static struct ring_buffer int_buffer = { { 0, 0, 0 } };

static atomic_val_t int_flag = 0;

static struct sol_ptr_vector opened_pins = SOL_PTR_VECTOR_INIT;

/* Zephyr structure used as a "bank of gpio pins" */
static struct device *device;

struct sol_gpio {
    int pin;
    bool active_low;
    void (*cb)(void *data, struct sol_gpio *gpio);
    const void *cb_data;
};

/* Run in interrupt context */
static void
gpio_isr_cb(struct device *port, uint32_t pin)
{
    unsigned int pin_as_decimal;

    /* We should not rewrite values, if there's no space on the buffer,
     * we ignore the incoming values until we process the olders */
    if (int_buffer.end + 1 == int_buffer.start)
        return;

    pin_as_decimal = 32 - __builtin_clz(pin) - 1;

    int_buffer.data[int_buffer.end] = pin_as_decimal;

    if (!atomic_cas(&int_buffer.end, RING_BUFFER_SIZE - 1, 0))
        atomic_add(&int_buffer.end, 1);

    atomic_set(&int_flag, 1);
}

void
sol_gpio_interrupt_process(void)
{
    struct sol_gpio *g;
    uint16_t i;

    if (!atomic_cas(&int_flag, 1, 0))
        return;

    while (int_buffer.start != int_buffer.end) {
        uint16_t pin = int_buffer.data[int_buffer.start];

        SOL_PTR_VECTOR_FOREACH_IDX (&opened_pins, g, i) {
            if (g->pin == pin && g->cb) {
                g->cb((void *)g->cb_data, g);
                break;
            }
        }

        if (!atomic_cas(&int_buffer.start, RING_BUFFER_SIZE - 1, 0))
            atomic_add(&int_buffer.start, 1);
    }
}

SOL_API struct sol_gpio *
sol_gpio_open_raw(int pin, const struct sol_gpio_config *config)
{
    struct sol_gpio *g, *gpio;
    int flags = 0;
    uint16_t i;

    SOL_LOG_INTERNAL_INIT_ONCE;

    if (unlikely(config->api_version != SOL_GPIO_CONFIG_API_VERSION)) {
        SOL_WRN("Couldn't open gpio that has unsupported version '%u', "
            "expected version is '%u'",
            config->api_version, SOL_GPIO_CONFIG_API_VERSION);
        return NULL;
    }

    if (opened_pins.base.len == 0) {
        if (!device) {
            device = device_get_binding("GPIO_0");
            SOL_NULL_CHECK(device, NULL);
        } else {
            if (gpio_resume(device) < 0) {
                SOL_WRN("Couldn't resume gpio");
                return NULL;
            }
        }
    } else {
        SOL_PTR_VECTOR_FOREACH_IDX (&opened_pins, g, i) {
            if (g->pin == pin) {
                SOL_WRN("GPIO pin %d is already opened", pin);
                return NULL;
            }
        }
    }

    gpio = malloc(sizeof(struct sol_gpio));
    SOL_NULL_CHECK(gpio, NULL);

    gpio->pin = pin;
    gpio->active_low = config->active_low;

    if (config->dir == SOL_GPIO_DIR_IN) {
        if (config->in.trigger_mode == SOL_GPIO_EDGE_NONE) {
            flags = flags | GPIO_DIR_IN;
            if (gpio_pin_configure(device, gpio->pin, flags) < 0) {
                SOL_WRN("Couldn't configure gpio");
                goto error;
            }
        } else {
            flags = flags
                | GPIO_INT
                | (config->dir == config->active_low ? GPIO_INT_ACTIVE_LOW : GPIO_INT_ACTIVE_HIGH)
                | (config->in.trigger_mode == SOL_GPIO_EDGE_BOTH ? GPIO_INT_DOUBLE_EDGE : GPIO_INT_EDGE);

            gpio->cb = config->in.cb;
            gpio->cb_data = (void *)config->in.user_data;

            if (gpio_pin_configure(device, gpio->pin, flags) < 0) {
                SOL_WRN("Couldn't configure gpio");
                goto error;
            }

            if (gpio_set_callback(device, gpio_isr_cb) < 0) {
                SOL_WRN("Couldn't set callback to gpio");
                goto error;
            }

            if (gpio_pin_enable_callback(device, gpio->pin) < 0) {
                SOL_WRN("Couldn't set callback to gpio");
                goto error;
            }
        }
    } else {
        if (gpio_pin_configure(device, gpio->pin, GPIO_DIR_OUT) < 0) {
            SOL_WRN("Couldn't configure gpio");
            goto error;
        }

        sol_gpio_write(gpio, config->out.value);
    }

    if (sol_ptr_vector_append(&opened_pins, gpio) < 0) {
        SOL_WRN("Couldn't configure gpio");
        goto error;
    }

    return gpio;

error:
    free(gpio);
    return NULL;
}

SOL_API void
sol_gpio_close(struct sol_gpio *gpio)
{
    struct sol_gpio *g;
    uint16_t i;

    SOL_NULL_CHECK(device);
    SOL_NULL_CHECK(gpio);

    SOL_PTR_VECTOR_FOREACH_IDX (&opened_pins, g, i) {
        if (g->pin == gpio->pin) {
            if (gpio_pin_disable_callback(device, g->pin) < 0) {
                SOL_WRN("Couldn't disable gpio callback");
            }
            sol_ptr_vector_del(&opened_pins, i);
            break;
        }
    }

    if (opened_pins.base.len == 0) {
        if (gpio_suspend(device) < 0) {
            SOL_WRN("Couldn't suspend gpio");
        }
    }

    free(gpio);
}

SOL_API bool
sol_gpio_write(struct sol_gpio *gpio, bool value)
{
    SOL_NULL_CHECK(device, false);
    SOL_NULL_CHECK(gpio, false);

    if (gpio_pin_write(device, gpio->pin, gpio->active_low ^ value) < 0) {
        SOL_WRN("Couldn't write to gpio pin:%d", gpio->pin);
        return false;
    }

    return true;
}

SOL_API int
sol_gpio_read(struct sol_gpio *gpio)
{
    uint32_t value;

    SOL_NULL_CHECK(device, -EINVAL);
    SOL_NULL_CHECK(gpio, -EINVAL);

    if (gpio_pin_read(device, gpio->pin, &value) < 0) {
        SOL_WRN("Couldn't read gpio pin:%d", gpio->pin);
        return -EINVAL;
    }

    return gpio->active_low ^ !!value;
}
