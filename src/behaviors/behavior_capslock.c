/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_capslock

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/hid_indicators.h>
#include <zmk/keys.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define HID_INDICATORS_CAPSLOCK_BIT BIT(1)

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct capslock_key_item {
    uint16_t page;
    uint32_t id;
    uint8_t implicit_modifiers;
};

struct behavior_capslock_config {
    uint8_t index;
    uint32_t capslock_press_keycode;
    uint32_t capslock_press_duration;
    bool enable_on_press;
    bool disable_on_release;
    bool disable_on_next_release;
    uint8_t disable_on_keys_count;
    struct capslock_key_item disable_on_keys[];
};

uint32_t config_capslock_press_keycode(const struct behavior_capslock_config *config) {
    return config->capslock_press_keycode > 0 ? config->capslock_press_keycode : CAPSLOCK;
}

struct behavior_capslock_data {
    struct zmk_behavior_binding_event event;
    bool active;
    bool just_activated;
};

static void toggle_capslock(const struct device *dev) {
    const struct behavior_capslock_config *config = dev->config;
    const struct behavior_capslock_data *data = dev->data;

    const int32_t keycode = config_capslock_press_keycode(config);
    const struct zmk_behavior_binding kp_capslock = {
        .behavior_dev = "key_press",
        .param1 = keycode,
    };

    LOG_DBG("queueing %dms capslock press (usage_page 0x%02X keycode 0x%02X)",
            config->capslock_press_duration, ZMK_HID_USAGE_PAGE(keycode),
            ZMK_HID_USAGE_ID(keycode));
    zmk_behavior_queue_add(&data->event, kp_capslock, true, config->capslock_press_duration);
    zmk_behavior_queue_add(&data->event, kp_capslock, false, 0);
}

static bool get_capslock_state(void) {
    return zmk_hid_indicators_get_current_profile() & HID_INDICATORS_CAPSLOCK_BIT;
}

static void set_capslock_state(const struct device *dev, const bool target_state) {
    const bool current_state = get_capslock_state();

    if (current_state != target_state) {
        LOG_DBG("toggling capslock state from %d to %d", current_state, target_state);
        toggle_capslock(dev);
    } else {
        LOG_DBG("capslock state is already %d", target_state);
    }
}

static void activate_capslock(const struct device *dev) {
    struct behavior_capslock_data *data = dev->data;

    set_capslock_state(dev, true);

    data->just_activated |= !data->active; // gets reset in `on_capslock_binding_released`
    data->active = true;
}

static void deactivate_capslock(const struct device *dev) {
    struct behavior_capslock_data *data = dev->data;

    set_capslock_state(dev, false);
    data->active = false;
}

static int on_capslock_binding_pressed(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_capslock_config *config = dev->config;
    struct behavior_capslock_data *data = dev->data;

    data->event = event;
    if (config->enable_on_press) {
        LOG_DBG("activating capslock (enable-on-press)");
        activate_capslock(dev);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_capslock_binding_released(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_capslock_config *config = dev->config;
    struct behavior_capslock_data *data = dev->data;

    if (config->disable_on_release) {
        LOG_DBG("deactivating capslock (disable-on-release)");
        deactivate_capslock(dev);
    } else if (config->disable_on_next_release && !data->just_activated) {
        LOG_DBG("deactivating capslock (disable-on-next-release)");
        deactivate_capslock(dev);
    }

    data->just_activated = false;

    return ZMK_BEHAVIOR_OPAQUE;
}

static bool capslock_match_key_item(const struct capslock_key_item *key_items,
                                    const size_t key_items_count, uint16_t usage_page,
                                    uint8_t usage_id, uint8_t implicit_modifiers) {
    for (int i = 0; i < key_items_count; ++i) {
        const struct capslock_key_item *break_item = &key_items[i];
        LOG_DBG("checking disable-on-keys: usage_page 0x%02X keycode 0x%02X", break_item->page,
                break_item->id);
        if (break_item->page == usage_page && break_item->id == usage_id &&
            (break_item->implicit_modifiers & (implicit_modifiers | zmk_hid_get_explicit_mods())) ==
                break_item->implicit_modifiers) {
            return true;
        }
    }

    return false;
}

static const struct device *devs[DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)];

static int capslock_keycode_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (int i = 0; i < DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT); i++) {
        const struct device *dev = devs[i];
        if (dev == NULL) {
            continue;
        }

        struct behavior_capslock_data *data = dev->data;
        if (!data->active) {
            continue;
        }

        const struct behavior_capslock_config *config = dev->config;

        if (ZMK_HID_USAGE(ev->usage_page, ev->keycode) == config_capslock_press_keycode(config)) {
            if (get_capslock_state()) {
                LOG_DBG("capslock being toggled off (capslock key event: usage_page 0x%02X keycode "
                        "0x%02X)",
                        ev->usage_page, ev->keycode);
                data->active = false;
            }
        } else if (capslock_match_key_item(config->disable_on_keys, config->disable_on_keys_count,
                                           ev->usage_page, ev->keycode, ev->implicit_modifiers)) {
            LOG_DBG("deactivating capslock (disable-on-keys: usage_page 0x%02X keycode 0x%02X)",
                    ev->usage_page, ev->keycode);
            deactivate_capslock(dev);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

static const struct behavior_driver_api behavior_capslock_driver_api = {
    .binding_pressed = on_capslock_binding_pressed,
    .binding_released = on_capslock_binding_released,
};

ZMK_LISTENER(behavior_capslock, capslock_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_capslock, zmk_keycode_state_changed);

static int behavior_capslock_init(const struct device *dev) {
    const struct behavior_capslock_config *config = dev->config;
    devs[config->index] = dev;
    return 0;
}

#define PARSE_KEY_ITEM(i)                                                                          \
    {                                                                                              \
        .page = ZMK_HID_USAGE_PAGE(i),                                                             \
        .id = ZMK_HID_USAGE_ID(i),                                                                 \
        .implicit_modifiers = SELECT_MODS(i),                                                      \
    }

#define KEY_ITEM(i, n) PARSE_KEY_ITEM(DT_INST_PROP_BY_IDX(n, disable_on_keys, i))

#define CAPSLOCK_INST(n)                                                                           \
    static struct behavior_capslock_data behavior_capslock_data_##n = {.active = false};           \
    static struct behavior_capslock_config behavior_capslock_config_##n = {                        \
        .index = n,                                                                                \
        .capslock_press_keycode = DT_INST_PROP(n, capslock_press_keycode),                         \
        .capslock_press_duration = DT_INST_PROP(n, capslock_press_duration),                       \
        .enable_on_press = DT_INST_PROP(n, enable_on_press),                                       \
        .disable_on_release = DT_INST_PROP(n, disable_on_release),                                 \
        .disable_on_next_release = DT_INST_PROP(n, disable_on_next_release),                       \
        .disable_on_keys = {LISTIFY(DT_INST_PROP_LEN(n, disable_on_keys), KEY_ITEM, (, ), n)},     \
        .disable_on_keys_count = DT_INST_PROP_LEN(n, disable_on_keys),                             \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_capslock_init, NULL, &behavior_capslock_data_##n,          \
                            &behavior_capslock_config_##n, POST_KERNEL,                            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_capslock_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CAPSLOCK_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY
