import re
from typing import Any, Optional


TEMPLATE = """
/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <dt-bindings/zmk/keys.h>

/ {
    behaviors {
__BEHAVIORS__
    };
};
"""


def main():
    configs: dict[tuple[str, str], dict[str, Any]] = {
        ("capslock_on", "cplkon"): {
            "enable-on-press": True,
            "display-name": '"Capslock on"',
        },
        ("capslock_off", "cplkoff"): {
            "disable-on-release": True,
            "display-name": '"Capslock off"',
        },
        ("capslock_hold", "cplkhld"): {
            "enable-on-press": True,
            "disable-on-release": True,
            "display-name": '"Capslock hold"',
        },
        ("capslock_word", "cplkwrd"): {
            "enable-on-press": True,
            "disable-on-next-release": True,
            "disable-on-keys": "<SPACE TAB ENTER>",
            "display-name": '"Capslock word"',
        },
        ("capslock_line", "cplkln"): {
            "enable-on-press": True,
            "disable-on-next-release": True,
            "disable-on-keys": "<ENTER>",
            "display-name": '"Capslock line"',
        },
    }

    default_configs = fill_configs(configs, press_ms=5)
    mac_configs = fill_configs(configs, press_ms=95, suffix=("_mac", "2", " (Mac)"))

    configs_code = (
        format_configs(default_configs),
        "        /* MacOS compatibility (longer capslock press) */",
        "",
        format_configs(mac_configs),
    )
    print(TEMPLATE.strip("\n").replace("__BEHAVIORS__", "\n".join(configs_code)))


def format_configs(configs: dict[tuple[str, str], dict[str, Any]], indent: int = 2):
    def lines():
        for (name, short_name), config in configs.items():
            yield indent, f"#if ZMK_BEHAVIOR_OMIT({name.upper()})"
            yield indent, "/omit-if-no-ref/"
            yield indent, "#endif"
            yield indent, f"{name}: {short_name} {{"
            # yield indent, f"/omit-if-no-ref/ {name}: {short_name} {{"
            for k, v in config.items():
                if v is True:
                    yield indent + 1, f"{k};"
                elif v not in (None, False):
                    yield indent + 1, f"{k} = {v};"
            yield indent, "};"
            yield indent, ""

    return "\n".join(("    " * i + line) if line else "" for i, line in lines())


def fill_configs(
    configs: dict[tuple[str, str], dict[str, Any]],
    compatible: str = "zmk,behavior-capslock",
    binding_cells: int = 0,
    press_ms: Optional[int] = None,
    suffix: tuple[str, str, str] = ("", "", ""),
):
    def update(config: dict[str, Any]):
        updated = {
            "compatible": f'"{compatible}"',
            "#binding-cells": f"<{binding_cells}>",
        }
        if press_ms is not None:
            updated["capslock-press-duration"] = f"<{press_ms}>"
        updated.update(**config)
        try:
            updated["display-name"] = re.sub(
                r'"$', f'{suffix[2]}"', updated["display-name"]
            )
        except KeyError:
            pass

        return updated

    return {
        (f"{name}{suffix[0]}", f"{short_name}{suffix[1]}"): update(config)
        for (name, short_name), config in configs.items()
    }


if __name__ == "__main__":
    main()
