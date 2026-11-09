# Core Runtime Contract

EPIC-02 introduces the public Core Runtime contract for Project Atlas.

The Runtime is the future entry point used by CLI, Doctor, Repair, Automation, TUI and future GUI layers.

---

## Goals

- Provide a stable public API above backend-specific code.
- Keep CLI and higher-level modules away from direct PipeWire, WirePlumber and ALSA details.
- Reuse the existing PpmContext, PpmRegistry and Core Models instead of replacing them prematurely.
- Preserve current CLI behavior while migration happens incrementally.

---

## Public Header

The initial contract lives in:

    include/core/ppm_runtime.h

---

## Runtime Object

PpmRuntime is opaque.

Users of the API should not access its internal fields directly.

    typedef struct PpmRuntime PpmRuntime;

---

## Runtime State

Initial states:

    PPM_RUNTIME_STATE_EMPTY
    PPM_RUNTIME_STATE_INITIALIZED
    PPM_RUNTIME_STATE_SCANNED
    PPM_RUNTIME_STATE_SHUTDOWN

These states are intentionally small. More may be added only when required.

---

## Initial API

    PpmResult ppm_runtime_create(PpmRuntime **runtime);
    PpmResult ppm_runtime_scan(PpmRuntime *runtime);
    PpmRuntimeState ppm_runtime_state(const PpmRuntime *runtime);
    void ppm_runtime_destroy(PpmRuntime *runtime);

---

## Implementation Strategy

This task defines the contract only.

Future tasks will implement the runtime gradually using existing modules:

- PpmContext
- PpmRegistry
- Core Models
- Backend adapters

No current CLI behavior should be broken during the transition.

---

## Status

Contract created.

Implementation pending.
