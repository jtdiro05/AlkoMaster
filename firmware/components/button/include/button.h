#pragma once

/*
 * Platform-independent button driver with debouncing and short/long press
 * detection (Dependency Inversion).
 *
 * The driver depends ONLY on the abstract GPIO/time interface button_io_t
 * defined below and includes only standard C headers. Any GPIO implementation
 * whose functions match the button_*_fn signatures can be plugged in, e.g.
 * the generic ESP-IDF GPIO adapter (esp32_gpio_adapter.h in esp32_adapters):
 *
 *   button_io_t field | ESP-IDF adapter symbol | handle type behind void*
 *   ------------------+------------------------+-------------------------
 *   handle            | &gpio_ctx              | esp32_gpio_ctx_t *
 *   read_level        | esp32_gpio_read_level  |
 *   get_time_ms       | esp32_get_time_ms      |
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_DEFAULT_DEBOUNCE_MS   30u   /**< Default contact debounce filter time [ms] */
#define BUTTON_DEFAULT_LONG_PRESS_MS 2000u /**< Default threshold for long press [ms] */

/**
 * @brief Read current logic level of the button pin
 * @param handle Opaque GPIO handle
 * @return 0 for logic LOW, 1 for logic HIGH, negative value on error
 */
typedef int (*button_read_level_fn)(void *handle);

/**
 * @brief Return monotonic system time in milliseconds [ms]
 */
typedef uint32_t (*button_get_time_ms_fn)(void);

/**
 * @brief Hardware abstraction interface injected into the button driver
 */
typedef struct {
    void *handle;                      /**< Opaque GPIO handle passed unchanged to read_level */
    button_read_level_fn read_level;   /**< Required: GPIO logic level reader (0 or 1) */
    button_get_time_ms_fn get_time_ms; /**< Required: monotonic clock [ms] */
} button_io_t;

/** Result codes of the button driver */
typedef enum {
    BUTTON_OK                =  0,
    BUTTON_ERR_NULL          = -1, /**< NULL pointer argument or missing read_level/get_time_ms callback */
    BUTTON_ERR_INVALID_ARG   = -2, /**< Invalid configuration (e.g. long_press_ms <= debounce_ms) */
    BUTTON_ERR_INVALID_STATE = -3, /**< Driver not initialized */
    BUTTON_ERR_IO            = -4  /**< GPIO read callback returned an error */
} button_err_t;

/** Button events emitted by button_update() */
typedef enum {
    BUTTON_EVENT_NONE = 0,    /**< No event in this polling cycle */
    BUTTON_EVENT_SHORT_PRESS, /**< Button was pressed and released before long_press_ms */
    BUTTON_EVENT_LONG_PRESS   /**< Button has been held continuously for long_press_ms */
} button_event_t;

/** Button behavior configuration */
typedef struct {
    bool active_low;        /**< true if pressed state is logic 0 (pull-up + switch to GND) */
    uint32_t debounce_ms;   /**< Debounce time [ms] (0 -> BUTTON_DEFAULT_DEBOUNCE_MS) */
    uint32_t long_press_ms; /**< Long press threshold [ms] (0 -> BUTTON_DEFAULT_LONG_PRESS_MS) */
} button_config_t;

/** Button instance descriptor (state machine) */
typedef struct {
    button_io_t io;         /**< Injected GPIO and clock interface */
    button_config_t config; /**< Timing and polarity configuration */
    bool raw_pressed;       /**< Last un-debounced logical pressed state */
    bool stable_pressed;    /**< Current debounced pressed state */
    bool long_reported;     /**< True if LONG_PRESS was already emitted for the current press */
    uint32_t last_change_ms;/**< Timestamp of the last raw state transition [ms] */
    uint32_t press_start_ms;/**< Timestamp when the debounced press started [ms] */
    bool initialized;       /**< Initialization status flag */
} button_t;

typedef button_t button_dev_t;

/**
 * @brief Initialize a button instance and bind the abstract GPIO interface
 *
 * @param[out] btn    Pointer to the button descriptor
 * @param[in]  io     Pointer to the GPIO/clock interface (read_level and get_time_ms must be non-NULL)
 * @param[in]  config Pointer to the button configuration
 * @return button_err_t BUTTON_OK on success
 */
button_err_t button_init(button_t *btn, const button_io_t *io, const button_config_t *config);

/**
 * @brief Poll the button state machine using the injected io.get_time_ms() callback
 *
 * @param[in,out] btn Pointer to the button descriptor
 * @return button_event_t Detected event (BUTTON_EVENT_NONE, SHORT_PRESS, or LONG_PRESS)
 */
button_event_t button_update(button_t *btn);

/**
 * @brief Return the current debounced pressed state
 *
 * @param[in] btn Pointer to the button descriptor
 * @return true if the button is currently pressed (after debounce), false otherwise
 */
bool button_is_pressed(const button_t *btn);

#ifdef __cplusplus
}
#endif
