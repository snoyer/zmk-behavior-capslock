# ZMK `capslock` behavior

This capslock behavior provides ways to create improved, configurable Caps Lock keys.
Pressing the regular Caps Lock key _toggles_ the state of caps lock on the host machine, making its effect dependent of said state.
This behavior can account for the current state to offer more control on how and when caps lock get activated and deactivated, regardless of the current state on the connected device.

## Behavior Binding

Five pre-configured instances are provided:

- `&capslock_on` enables caps lock (and does nothing if it's already on)
- `&capslock_off` disables caps lock (and does nothing if it's already off)
- `&capslock_hold` enables caps lock while held, and disables it when released (similar to a Shift key)
- `&capslock_word` enables caps lock until `SPACE`, `TAB`, or `ENTER` is typed or the key is pressed again
- `&capslock_line` enables caps lock until `ENTER` is typed or the key is pressed again

In order to ensure compatibility with MacOS (which ignores short capslock presses by design), `&capslock_on_mac`, `&capslock_off_mac`, `&capslock_hold_mac`, `&capslock_word_mac`, `&capslock_line_mac` versions using a longer press duration of 95ms are also available.

## Configuration

The following options allow to customize the capslock behavior:

- `enable-on-press`: enable caps lock when the key is pressed
- `disable-on-release`: disable caps lock when the key is released
- `disable-on-next-release`: disable caps lock when the key is released a second time
- `disable-on-keys`: list of keys after which to disable capslock
- `capslock-press-duration`: duration of the capslock key press to generate (in milliseconds)

## Examples

The pre-configured `capslock_word` (which enables caps lock when pressed, and disables it when `SPACE`, `TAB`, or `ENTER` is pressed, or the key is pressed again) can be defined as follow:

```
capslock_word: behavior_capslock_word {
    compatible = "zmk,behavior-capslock";
    #binding-cells = <0>;
    capslock-press-ms = <5>;
    enable-on-press;
    disable-on-next-release;
    disable-on-keys = <SPACE TAB ENTER>;
};
```
