# Core Architecture Report

Generated: Sun Jul  5 10:13:23 AM AST 2026

---

## Purpose

This report captures the current state of the Core Runtime area before starting EPIC-02 implementation work.

The goal is to understand what already exists, what is only a stub, and what should become part of the future Core Runtime.

---

## Current Core Headers

```text
include/core/ppm_core.h
include/core/ppm_hardware.h
include/core/ppm_models.h
include/core/ppm_profile.h
include/core/ppm_registry.h
include/core/ppm_workspace.h
```

---

## Current Core Sources

```text
src/core/ppm_hardware_core.c
src/core/ppm_models.c
src/core/ppm_models.o
src/core/ppm_profile_core.c
src/core/ppm_registry_core.c
src/core/ppm_workspace_core.c
```

---

## Initial Assessment

| Module | Header | Source | Status | Notes |
|---|---|---|---|---|
| Core umbrella | include/core/ppm_core.h | - | Partial | Umbrella header only. |
| Hardware Core | include/core/ppm_hardware.h | src/core/ppm_hardware_core.c | Stub/Partial | Needs responsibility review. |
| Models | include/core/ppm_models.h | src/core/ppm_models.c | Partial | Existing model layer; must be reviewed before replacing. |
| Profile Core | include/core/ppm_profile.h | src/core/ppm_profile_core.c | Stub/Partial | Needs contract definition. |
| Registry Core | include/core/ppm_registry.h | src/core/ppm_registry_core.c | Stub/Partial | Candidate center of EPIC-02. |
| Workspace Core | include/core/ppm_workspace.h | src/core/ppm_workspace_core.c | Stub/Partial | Needs relation to profiles and routing. |

---

## Non-Core Modules That May Interact With Core

These modules currently live outside `src/core` but may influence the Core Runtime design:

```text
include/ppm_context.h
include/ppm_graph.h
include/ppm_node.h
include/ppm_port.h
include/ppm_link.h
include/ppm_registry.h
include/ppm_device.h
include/ppm_hardware_db.h
include/ppm_profile_state.h
include/ppm_default.h
include/ppm_metadata.h
include/ppm_volume.h
```

---

## EPIC-02 Working Questions

1. What is the root runtime object?
2. Should the existing `ppm_context` become the public runtime entry point?
3. Which existing graph objects belong to Core models?
4. Which code is backend-specific and must stay outside Core?
5. Which Registry implementation should become the source of truth?
6. How should the Core expose read-only state to CLI, Doctor and Repair?
7. What should remain compatible with the current CLI during migration?

---

## Proposed EPIC-02 Direction

Start with inspection and consolidation, not replacement.

Recommended order:

1. Inspect current `ppm_context`, `ppm_registry`, and graph model files.
2. Define the minimal public Core Runtime API.
3. Keep existing CLI behavior working.
4. Gradually route commands through the new Core.
5. Add unit tests only after the API contract is clear.

---

## Current Health Requirement

Before modifying Core code, the project must remain in this state:

- Build: PASS
- Unit Tests: PASS
- Smoke Tests: PASS
- Known warnings only: profile.c snprintf truncation warnings

