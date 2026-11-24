/*
 * Platform-independent button state machine (debouncing + short/long press).
 * Only standard C headers are used; all I/O goes through btn->io.
 */
#include "button.h"

button_err_t button_init(button_t *btn, const button_io_t *io, const button_config_t *config)
{
    if (btn == NULL || io == NULL || config == NULL || io->read_level == NULL || io->get_time_ms == NULL) {
        return BUTTON_ERR_NULL;
    }

    uint32_t debounce = (config->debounce_ms > 0u) ? config->debounce_ms : BUTTON_DEFAULT_DEBOUNCE_MS;
    uint32_t long_ms  = (config->long_press_ms > 0u) ? config->long_press_ms : BUTTON_DEFAULT_LONG_PRESS_MS;
    if (long_ms <= debounce) {
        return BUTTON_ERR_INVALID_ARG;
    }

    *btn = (button_t){ 0 };
    btn->io = *io;
    btn->config = *config;
    btn->config.debounce_ms = debounce;
    btn->config.long_press_ms = long_ms;
    btn->initialized = true;
    return BUTTON_OK;
}

static button_event_t button_poll(button_t *btn, uint32_t now_ms)
{
    int level = btn->io.read_level(btn->io.handle);
    if (level < 0) {
        return BUTTON_EVENT_NONE;
    }

    bool pressed_now = btn->config.active_low ? (level == 0) : (level != 0);

    /* Reset debounce timer whenever the raw pin state changes */
    if (pressed_now != btn->raw_pressed) {
        btn->raw_pressed = pressed_now;
        btn->last_change_ms = now_ms;
    }

    /* Wait until the raw level stays stable for debounce_ms */
    if ((uint32_t)(now_ms - btn->last_change_ms) >= btn->config.debounce_ms) {
        if (btn->stable_pressed != btn->raw_pressed) {
            btn->stable_pressed = btn->raw_pressed;

            if (btn->stable_pressed) {
                /* Transition: RELEASED -> PRESSED */
                btn->press_start_ms = now_ms;
                btn->long_reported = false;
            } else {
                /* Transition: PRESSED -> RELEASED */
                if (!btn->long_reported) {
                    return BUTTON_EVENT_SHORT_PRESS;
                }
            }
        }
    }

    /* While stably held down, fire LONG_PRESS once when threshold is reached */
    if (btn->stable_pressed && !btn->long_reported) {
        if ((uint32_t)(now_ms - btn->press_start_ms) >= btn->config.long_press_ms) {
            btn->long_reported = true;
            return BUTTON_EVENT_LONG_PRESS;
        }
    }

    return BUTTON_EVENT_NONE;
}

button_event_t button_update(button_t *btn)
{
    if (btn == NULL || !btn->initialized || btn->io.get_time_ms == NULL) {
        return BUTTON_EVENT_NONE;
    }
    return button_poll(btn, btn->io.get_time_ms());
}

bool button_is_pressed(const button_t *btn)
{
    return (btn != NULL && btn->initialized) ? btn->stable_pressed : false;
}
