# Audio Profile Manager

# Core API Specification

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Purpose

The Core API defines the internal contracts used by every subsystem.

The CLI, future TUI, automation engine, doctor, repair system and all
backends communicate only through this API.

No backend should expose raw shell output.

-------------------------------------------------------------------------------

# Layered Architecture

                CLI
                 │
                TUI
                 │
          Automation Engine
                 │
          Doctor / Repair
                 │
              Core API
                 │
    ┌────────────┼────────────┐
    │            │            │
    ▼            ▼            ▼
  ALSA      PipeWire    WirePlumber

-------------------------------------------------------------------------------

# Core Objects

The API revolves around the following objects:

Hardware

Device

Node

Port

Link

Stream

MixerControl

Profile

Workspace

Capability

Registry

-------------------------------------------------------------------------------

# PpmRegistry

The Registry represents the complete live audio environment.

typedef struct
{
    PpmHardware hardware[32];

    int hardware_count;

    PpmCoreNode nodes[512];

    int node_count;

    PpmCorePort ports[1024];

    int port_count;

    PpmCoreLink links[1024];

    int link_count;

    PpmStream streams[256];

    int stream_count;

} PpmRegistry;

Every command should eventually operate on a PpmRegistry instance.

-------------------------------------------------------------------------------

# Registry Lifecycle

Future API:

ppm_registry_create()

↓

ppm_registry_refresh()

↓

Use objects

↓

ppm_registry_destroy()

This allows all commands to share the same data model.

-------------------------------------------------------------------------------

# PpmHardware

Represents one physical sound device.

Fields:

- id
- name
- chip
- model
- alsa_card
- capabilities
- controls
- playback nodes
- capture nodes

-------------------------------------------------------------------------------

# PpmMixerControl

Represents one ALSA control.

Fields:

- name

- type

- readable

- writable

- minimum

- maximum

- values

- recommended values

Future versions should never manipulate ALSA text directly.

-------------------------------------------------------------------------------

# PpmStream

Represents one application stream.

Examples:

Firefox

Discord

RetroArch

MPD

SPCPlay

Fields:

- node id

- application name

- sink

- volume

- mute

- channels

- sample rate

-------------------------------------------------------------------------------

# PpmProfile

Represents a complete audio profile.

Contains:

Hardware

↓

Mixer

↓

Metadata

↓

Streams

↓

Links

↓

Effects

↓

Automation metadata


-------------------------------------------------------------------------------

# Public Core API

The following functions represent the long-term public API of Project Atlas.

All frontends should use these functions.

-------------------------------------------------------------------------------

Registry API

ppm_registry_create()

Creates a new Registry object.

Responsibilities:

- Initialize memory.
- Connect to PipeWire.
- Initialize backends.

Returns:

PpmRegistry*

-------------------------------------------------------------------------------

ppm_registry_refresh()

Refreshes all Registry objects.

Updates:

- Hardware
- Devices
- Nodes
- Ports
- Links
- Streams
- Metadata

Should not destroy existing objects whenever possible.

-------------------------------------------------------------------------------

ppm_registry_destroy()

Releases every resource associated with the Registry.

Responsibilities:

- Free memory.
- Disconnect PipeWire.
- Release WirePlumber objects.
- Close ALSA handles.

-------------------------------------------------------------------------------

Hardware API

ppm_hardware_scan()

Discovers every available sound device.

Produces:

PpmHardware objects.

Future:

Generates Hardware DB automatically.

-------------------------------------------------------------------------------

ppm_hardware_load()

Loads a hardware definition from:

hardware/<id>/

-------------------------------------------------------------------------------

ppm_hardware_save()

Stores current hardware information.

Used by:

hardware-scan

-------------------------------------------------------------------------------

Mixer API

ppm_mixer_export()

Exports useful ALSA controls into JSON.

Purpose:

Avoid depending on full asound.state.

-------------------------------------------------------------------------------

ppm_mixer_import()

Restores useful controls.

Should ignore unsupported controls.

-------------------------------------------------------------------------------

ppm_mixer_compare()

Compares:

Current controls

↓

Recommended controls

Returns differences.

-------------------------------------------------------------------------------

Profile API

ppm_profile_save()

Creates a profile.

Current implementation:

Stores:

- ALSA
- PipeWire
- Metadata
- Links

Future:

Stores Core objects.

-------------------------------------------------------------------------------

ppm_profile_load()

Loads one profile.

Future:

Selective restoration.

-------------------------------------------------------------------------------

ppm_profile_compare()

Compares two profiles.

Example:

music

↓

movie

Outputs:

Different controls

Different routing

Different streams

-------------------------------------------------------------------------------

Doctor API

ppm_doctor_run()

Produces:

Complete diagnostic report.

Checks:

Hardware

Registry

Metadata

Mixer

Profiles

Routing

-------------------------------------------------------------------------------

Repair API

ppm_repair_run()

Repairs only modified values.

Never overwrites everything blindly.

-------------------------------------------------------------------------------

Automation API

ppm_rule_load()

Loads automation rules.

-------------------------------------------------------------------------------

ppm_rule_execute()

Executes matching rules.

-------------------------------------------------------------------------------

Workspace API

ppm_workspace_load()

Loads:

Hardware profile

User profile

Automation

Effects

-------------------------------------------------------------------------------

JSON API

Project Atlas stores structured objects.

Every object must support:

Load JSON

↓

Object

↓

Modify

↓

Save JSON

No module should manipulate JSON manually.

Future helpers:

ppm_json_load()

ppm_json_save()

ppm_json_validate()

-------------------------------------------------------------------------------

Error Handling

Every public API returns:

0

Success

Negative values

Failure

Example:

0

Success

-1

Invalid argument

-2

Backend unavailable

-3

Permission denied

-4

Object not found

-5

Unsupported capability

-------------------------------------------------------------------------------

Logging

Future logger:

ppm_log()

Levels:

DEBUG

INFO

WARNING

ERROR

FATAL

Every backend should report meaningful errors.

-------------------------------------------------------------------------------

API Stability

The Core API should remain stable.

Backends may evolve independently.

This guarantees:

CLI compatibility

TUI compatibility

Future GUI compatibility

Plugin compatibility

-------------------------------------------------------------------------------

Summary

Project Atlas separates:

Core

↓

Objects

↓

Backends

↓

Operating System

The Core never depends directly on ALSA, PipeWire or WirePlumber.

Instead, it depends only on well-defined internal objects.

This makes the architecture maintainable, extensible and portable.

