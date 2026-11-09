# Audio Profile Manager

# Development Guide

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Introduction

This document defines the official development rules for
Audio Profile Manager (Project Atlas).

The goal is to keep the project maintainable even after many years
of development.

Every contributor should follow these guidelines.

-------------------------------------------------------------------------------

# Philosophy

Audio Profile Manager is NOT a collection of shell scripts.

It is a software platform.

The project must always evolve around:

Core

↓

Objects

↓

Backends

↓

Operating System

Never the opposite.

-------------------------------------------------------------------------------

# Architecture First

Every new feature must respect the frozen architecture.

Never add functionality directly inside main().

Instead:

CLI

↓

Core API

↓

Backend

↓

Operating System

-------------------------------------------------------------------------------

# Object Oriented Design

Although the project is written in C,
the architecture follows an object-oriented model.

Examples:

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

These objects are the language of the project.

-------------------------------------------------------------------------------

# Core Responsibilities

The Core owns:

Profiles

Hardware model

Diagnostics

Automation

Workspace logic

Registry cache

Future TUI

The Core MUST NOT know shell commands.

-------------------------------------------------------------------------------

# Backend Responsibilities

Backends communicate with Linux.

Current backends:

ALSA

PipeWire

WirePlumber

Exec

Future:

EasyEffects

Carla

JACK

Pulse compatibility

-------------------------------------------------------------------------------

# Shell Commands

Never execute shell commands directly.

Incorrect:

system("wpctl set-default 52");

Incorrect:

popen(...)

Correct:

ppm_exec(...)

Future preferred:

ppm_backend_wireplumber_set_default()

-------------------------------------------------------------------------------

# Structured Data

Never parse text twice.

Convert external information into Core objects.

Bad:

wpctl

↓

text

↓

print

Good:

wpctl

↓

Backend

↓

Objects

↓

Core

↓

CLI

-------------------------------------------------------------------------------

# JSON

JSON is a persistence format.

It is NOT the internal representation.

Internal representation:

Objects

Persistent representation:

JSON

-------------------------------------------------------------------------------

# Hardware Database

Hardware definitions belong in:

hardware/

Never hardcode device-specific logic inside the Core.

If special behavior is required:

hardware/<device>/driver.c

hardware/<device>/driver.h

-------------------------------------------------------------------------------

# Documentation

Every new subsystem must be documented.

Documentation belongs in:

docs/

No undocumented subsystem should be merged.

-------------------------------------------------------------------------------

# Testing

Every feature should be tested before committing.

Minimum build:

make clean

make

Basic verification:

./audio-profile devices

./audio-profile graph

Hardware verification:

wpctl status

alsamixer

-------------------------------------------------------------------------------

# Commit Messages

Preferred format:

Component:

Short description

Examples:

Core:
Add Registry cache

Hardware:
Add CA0132 recommendations

Doctor:
Detect invalid defaults

-------------------------------------------------------------------------------

# Code Review

Before merging:

Does this belong in the Core?

Does this belong in a Backend?

Can this become a Hardware Driver?

Can this become an Automation Rule?

Can this become reusable?


-------------------------------------------------------------------------------

# Naming Conventions

Project Atlas follows a strict naming convention.

Structures:

PpmHardware

PpmRegistry

PpmStream

PpmProfile

PpmNode

PpmPort

PpmLink

Functions:

ppm_registry_create()

ppm_registry_refresh()

ppm_registry_destroy()

ppm_profile_save()

ppm_profile_load()

Files:

ppm_registry.c

ppm_registry.h

ppm_stream.c

ppm_stream.h

-------------------------------------------------------------------------------

# Directory Layout

The source tree is organized by subsystem.

src/

    core/

    alsa/

    pipewire/

    wireplumber/

    hardware/

    profiles/

    doctor/

    automation/

    tui/

Each directory owns its own functionality.

Avoid mixing unrelated modules.

-------------------------------------------------------------------------------

# Header Organization

Every public module must expose a header.

Example:

include/

    core/

        ppm_models.h

        ppm_registry.h

        ppm_profile.h

Implementation belongs under:

src/core/

-------------------------------------------------------------------------------

# Memory Management

Rules:

• Every create() has a destroy().

• Ownership must be explicit.

• Avoid hidden allocations.

• Prefer stack allocation whenever practical.

Future dynamic containers should document ownership clearly.

-------------------------------------------------------------------------------

# Error Handling

Every public function returns:

0

Success

Negative values

Failure

Example:

0

Operation completed.

-1

Invalid argument.

-2

Backend unavailable.

-3

Permission denied.

-4

Object not found.

-5

Unsupported capability.

Never ignore backend failures silently.

-------------------------------------------------------------------------------

# Logging

Future logger:

ppm_log()

Supported levels:

DEBUG

INFO

WARNING

ERROR

FATAL

Every backend should provide meaningful log messages.

The Core should never print directly except through approved UI layers.

-------------------------------------------------------------------------------

# Coding Style

Preferred formatting:

int ppm_example(const char *name)
{
    if(!name)
        return -1;

    return 0;
}

Rules:

- Opening brace on next line.
- Four spaces indentation.
- Descriptive names.
- Small focused functions.
- Avoid deeply nested logic.

-------------------------------------------------------------------------------

# Backends

Every backend should expose a consistent interface.

ALSA Backend

Responsibilities:

- Mixer controls
- Hardware capabilities
- Safe restore
- Export

PipeWire Backend

Responsibilities:

- Registry
- Devices
- Nodes
- Ports
- Links
- Streams

WirePlumber Backend

Responsibilities:

- Metadata
- Defaults
- Routing
- Policies

Exec Backend

Responsibilities:

- Execute external tools.
- Capture output.
- Capture errors.
- Return structured results.

-------------------------------------------------------------------------------

# Hardware Drivers

Hardware-specific logic belongs inside:

hardware/<device>/

Example:

hardware/ca0132/

driver.c

driver.h

Possible functions:

ca0132_detect()

ca0132_apply_music()

ca0132_apply_movie()

ca0132_apply_game()

ca0132_doctor()

ca0132_repair()

The Core should never contain device-specific logic.

-------------------------------------------------------------------------------

# Documentation Requirements

Every new subsystem requires:

Architecture update.

API update.

Development notes.

Examples.

If the design changes, update:

docs/ARCHITECTURE.md

docs/API.md

docs/DEVELOPMENT.md

before merging.

-------------------------------------------------------------------------------

# Testing Strategy

Every commit should pass:

Build

make clean

make

Smoke tests

./audio-profile devices

./audio-profile graph

./audio-profile streams

Hardware verification

wpctl status

alsamixer

Regression testing should be added for future releases.

-------------------------------------------------------------------------------

# Versioning

Recommended version scheme:

v0.x

Experimental

v1.x

Stable API

v2.x

Major architectural changes

Architecture freezes should be tagged in Git.

-------------------------------------------------------------------------------

# Long-Term Goal

Project Atlas is intended to become a complete Linux audio management platform.

Future capabilities include:

• Hardware discovery

• Hardware database generation

• Profile management

• Diagnostics

• Repair

• Automation

• Terminal UI

• Plugin system

The guiding principle is simple:

Objects first.

Backends second.

Operating system last.

