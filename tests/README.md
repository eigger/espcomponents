# Component tests

ESPHome-style test configurations, one directory per component, mirroring the
layout of the upstream [`esphome/esphome`](https://github.com/esphome/esphome)
repository (`tests/components/<component>/`).

```
tests/components/<component>/
  common.yaml              # shared, board-agnostic config (only when >1 target)
  test.<platform>-<framework>.yaml
```

## Naming

Entry files are named `test.<platform>-<framework>.yaml`, e.g.:

- `test.esp32-idf.yaml`
- `test.esp8266-ard.yaml`
- `test.esp32-ard.yaml`

The CI matrix discovers every `tests/components/<component>/test.*.yaml`
(`common.yaml` is **not** an entry file — it is pulled in via `<<: !include`).

## Structure

Each entry file is self-contained and can be validated/compiled directly:

```bash
esphome config  tests/components/axp192/test.esp32-idf.yaml
esphome compile tests/components/axp192/test.esp32-idf.yaml
```

The component is loaded from this repo via a **local** source — no network /
GitHub fetch and no CI patching:

```yaml
external_components:
  - source:
      type: local
      path: ../../../components
    components: [ <component> ]
```

### Multiple targets

When a component is tested on more than one platform, the board-agnostic part
lives in `common.yaml` and each target file only sets the board and any
board-specific pins via `substitutions`:

```yaml
# test.esp32-idf.yaml
substitutions:
  sda: GPIO21
  scl: GPIO22

esp32:
  board: esp32dev
  framework:
    type: esp-idf

<<: !include common.yaml
```

```yaml
# common.yaml
external_components:
  - source:
      type: local
      path: ../../../components
    components: [ axp192 ]

esphome:
  name: test-axp192

i2c:
  - id: bus_a
    sda: ${sda}
    scl: ${scl}
    scan: true

axp192:
  # ...
```

### Single target

Components tested on a single platform skip `common.yaml` and keep one
self-contained entry file (e.g. `test.esp32-idf.yaml`).

## CI

`.github/workflows/esphome.yml` detects components changed under `components/`
or `tests/components/`, then validates and compiles each matching
`test.*.yaml`. A component modified under `components/` **must** have at least
one test file here or CI fails.
