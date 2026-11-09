# Audio Profile Manager

# Project Atlas Architecture

Version: 0.3

Status: Frozen Architecture

---

# Overview

Project Atlas is the long-term architecture of Audio Profile Manager.

Its purpose is to provide a unified management layer above:

- ALSA
- PipeWire
- WirePlumber

Instead of exposing individual commands, the project models the complete audio environment using internal objects.

---

# High Level Architecture

                        Audio Profile Manager

                                │

        ┌───────────────────────┼────────────────────────┐

        │                       │                        │

        ▼                       ▼                        ▼

      Core                Hardware DB             Diagnostics

        │                       │                        │

        ▼                       ▼                        ▼

    ALSA Backend         PipeWire Backend      WirePlumber Backend

        │                       │                        │

        └───────────────┬───────┴──────────────┬─────────┘

                        ▼

                 Automation Engine

                        ▼

                        TUI

---

# Design Philosophy

Project Atlas follows a very simple rule:

The Core should never know how the system works internally.

Instead, the Core manipulates objects.

Backends are responsible for communicating with:

- ALSA
- PipeWire
- WirePlumber

This separation keeps the code modular and easier to maintain.

---

# Core Objects

Everything inside the application revolves around a small number of objects.

Hardware

↓

Device

↓

Node

↓

Port

↓

Link

↓

Stream

↓

MixerControl

↓

Profile

↓

Workspace

↓

Automation Rule

These objects become the "language" of the project.

Backends translate between Linux audio APIs and these objects.

---

# Responsibilities

Core

Responsible for:

- Profiles
- Hardware abstraction
- Object management
- Diagnostics
- Automation
- Future GUI/TUI

The Core must never execute shell commands directly.

---

# Backends

Backends provide access to external systems.

Current backends:

• ALSA

Responsible for:

- Mixer controls
- Volume controls
- Hardware capabilities

Future:

Native ALSA API.

Current:

amixer
alsactl

---

• PipeWire

Responsible for:

- Registry
- Devices
- Nodes
- Ports
- Links
- Streams
- Graph

Future:

Native PipeWire API.

Current:

pw-dump
pw-link

---

• WirePlumber

Responsible for:

- Metadata
- Default devices
- Stream routing

Future:

Native WirePlumber API.

Current:

wpctl
pw-metadata

---

• Exec Backend

Responsible for executing external commands.

Current implementation:

ppm_exec()

Future:

Process execution layer with logging and structured results.


-------------------------------------------------------------------------------

# Core Layer

The Core is the heart of Audio Profile Manager.

It is intentionally independent from:

- ALSA
- PipeWire
- WirePlumber
- Desktop environments

The Core manipulates only internal objects.

No backend-specific code should leak into the Core.

-------------------------------------------------------------------------------

# Internal Object Model

The Core defines a set of canonical objects.

These objects are shared by:

- CLI
- Future TUI
- Future GUI
- Doctor
- Repair
- Automation
- Hardware Scanner
- Profile Engine

-------------------------------------------------------------------------------

Hardware

Represents one physical audio device.

Example:

    Creative Sound Blaster Z

Fields include:

- id
- name
- chip
- model
- alsa_card
- capabilities
- mixer controls
- playback nodes
- capture nodes

-------------------------------------------------------------------------------

Capability

Capabilities describe what a hardware device can do.

Examples:

    5.1 Output

    7.1 Output

    SPDIF

    Bass Redirection

    Hardware EQ

    Headphone Virtualization

Capabilities allow the program to make intelligent decisions without hardcoding
behavior for every supported sound card.

-------------------------------------------------------------------------------

MixerControl

Represents one ALSA mixer control.

Fields:

- name
- type
- readable
- writable
- minimum
- maximum
- current value(s)
- recommended value(s)

Future profile restoration should use MixerControl objects instead of restoring
the entire ALSA state.

-------------------------------------------------------------------------------

Device

Represents one PipeWire device.

Examples:

    Sound Blaster Z

    USB DAC

    Webcam microphone

A Device may expose one or more Nodes.

-------------------------------------------------------------------------------

Node

Represents one PipeWire node.

Examples:

Playback node

Capture node

Virtual sink

Virtual source

Application stream

-------------------------------------------------------------------------------

Port

Represents one PipeWire port.

Examples:

Front Left

Front Right

Center

LFE

Capture Left

Capture Right

Ports belong to Nodes.

-------------------------------------------------------------------------------

Link

Represents a PipeWire connection.

Links connect:

Output Port

↓

Input Port

Links are essential to understand routing.

-------------------------------------------------------------------------------

Stream

Represents one active application stream.

Examples:

MPD

Firefox

RetroArch

Discord

SPCPlay

A Stream includes:

- application name
- node id
- target sink
- sample rate
- channels
- volume
- mute

-------------------------------------------------------------------------------

Profile

Represents a complete audio configuration.

A Profile contains:

Hardware

+

Mixer controls

+

Default sink/source

+

Metadata

+

Routing

+

Effects

+

Recommendations

Profiles should become portable JSON objects.

-------------------------------------------------------------------------------

Workspace

A Workspace is a higher-level concept.

Examples:

Music

Gaming

Movie

Retro

Streaming

A Workspace may contain:

Multiple profiles

Automation rules

Application routing

DSP presets

Effects

-------------------------------------------------------------------------------

Automation Rule

Automation Rules describe behavior.

Example:

When:

Application == "SPCPlay"

Then:

Load Workspace "Retro"

Future versions will support event-driven automation.

-------------------------------------------------------------------------------

Registry Cache

One of the main architectural goals is to eliminate repeated PipeWire scans.

Current situation:

Every command rebuilds its own view of the Registry.

Future:

PipeWire

↓

Registry

↓

Registry Cache

↓

Devices

↓

Streams

↓

Doctor

↓

Profiles

↓

TUI

All components will share the same Registry Cache.

-------------------------------------------------------------------------------

Advantages

A Registry Cache provides:

- Faster execution
- Consistent data
- Reduced PipeWire queries
- Simpler diagnostics
- Easier GUI/TUI implementation

