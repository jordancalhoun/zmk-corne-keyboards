/*
 * Fades the Prospector backlight with ZMK's activity state. ZMK's display
 * blanking only sends DISP_OFF to the panel, which leaves the backlight on.
 */

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

#define ACTIVE_BRIGHTNESS CONFIG_PROSPECTOR_FIXED_BRIGHTNESS
#define IDLE_BRIGHTNESS CONFIG_CORNE_DONGLE_BACKLIGHT_IDLE_BRIGHTNESS
#define FADE_RANGE MAX(1, abs(ACTIVE_BRIGHTNESS - IDLE_BRIGHTNESS))
#define FADE_STEP_MS MAX(1, CONFIG_CORNE_DONGLE_BACKLIGHT_FADE_MS / FADE_RANGE)

// prospector's fixed brightness init has already set this at boot
static uint8_t current_brightness = ACTIVE_BRIGHTNESS;
static uint8_t target_brightness = ACTIVE_BRIGHTNESS;

static void fade_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(fade_work, fade_work_handler);

static void fade_work_handler(struct k_work *work) {
    if (current_brightness == target_brightness) {
        return;
    }

    if (CONFIG_CORNE_DONGLE_BACKLIGHT_FADE_MS == 0) {
        current_brightness = target_brightness;
    } else if (current_brightness < target_brightness) {
        current_brightness++;
    } else {
        current_brightness--;
    }

    led_set_brightness(pwm_leds_dev, DISP_BL, current_brightness);

    if (current_brightness != target_brightness) {
        k_work_reschedule(&fade_work, K_MSEC(FADE_STEP_MS));
    }
}

static int backlight_idle_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    target_brightness =
        ev->state == ZMK_ACTIVITY_ACTIVE ? ACTIVE_BRIGHTNESS : IDLE_BRIGHTNESS;
    LOG_DBG("backlight target %d", target_brightness);
    k_work_reschedule(&fade_work, K_NO_WAIT);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(backlight_idle, backlight_idle_listener);
ZMK_SUBSCRIPTION(backlight_idle, zmk_activity_state_changed);
