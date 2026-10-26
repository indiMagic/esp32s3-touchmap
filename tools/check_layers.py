#!/usr/bin/env python3
"""Enforce the firmware layering rules by scanning #include lines.

    main  ->  ui  ->  kernel  ->  hw_hal  ->  ESP-IDF drivers

Rules
  * hw_hal may not include kernel or ui headers.
  * kernel may not include ui headers.
  * ui may not include hw_hal headers or low-level driver/ESP-IDF peripheral
    headers (it talks to hardware only through kernel.h).
  * Portable kernel files (proto, geo, k_tpk) may not include ESP-IDF/FreeRTOS
    headers, so they stay unit-testable on the host.

Exit status is non-zero if any rule is broken.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
COMP = ROOT / "components"

INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def headers_of(component):
    inc = COMP / component / "include"
    return {p.name for p in inc.glob("*.h")} if inc.exists() else set()


HAL_H = headers_of("hw_hal")
KERNEL_H = headers_of("kernel")
UI_H = headers_of("ui")

LOW_LEVEL_PREFIXES = (
    "driver/", "esp_lcd", "esp_vfs", "sdmmc", "diskio", "ff.h", "nimble/", "host/",
    "services/", "soc/", "hal/", "esp_rom", "esp_heap_caps", "nvs",
)

PORTABLE = {"k_proto.c", "k_proto.h", "k_geo.c", "k_geo.h", "k_tpk.c", "k_tpk.h"}
ESP_HEADER_PREFIXES = ("esp_", "freertos/", "driver/", "nvs", "soc/")


def rules_for(component, path):
    """Return a list of (predicate, message) for one source file."""
    rules = []
    if component == "hw_hal":
        rules.append((lambda h: h in KERNEL_H or h in UI_H, "hw_hal must not include upper layers"))
    elif component == "kernel":
        rules.append((lambda h: h in UI_H, "kernel must not include ui headers"))
        if path.name in PORTABLE:
            rules.append((lambda h: h.startswith(ESP_HEADER_PREFIXES),
                          "portable kernel file must not include ESP-IDF headers"))
    elif component == "ui":
        rules.append((lambda h: h in HAL_H, "ui must not include hw_hal headers (use kernel.h)"))
        rules.append((lambda h: h.startswith(LOW_LEVEL_PREFIXES),
                      "ui must not include low-level ESP-IDF headers"))
    return rules


def main():
    bad = 0
    for comp_dir in sorted(p for p in COMP.iterdir() if p.is_dir()):
        comp = comp_dir.name
        for path in sorted(list(comp_dir.rglob("*.c")) + list(comp_dir.rglob("*.h"))):
            rules = rules_for(comp, path)
            for n, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
                m = INCLUDE.match(line)
                if not m:
                    continue
                hdr = m.group(1)
                for pred, msg in rules:
                    if pred(hdr):
                        print(f"{path.relative_to(ROOT)}:{n}: {msg} ({hdr})")
                        bad += 1
    if bad:
        print(f"\n{bad} layering violation(s)")
        return 1
    print("layering OK: main -> ui -> kernel -> hw_hal")
    return 0


if __name__ == "__main__":
    sys.exit(main())
