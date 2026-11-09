# Audio Profile Manager

# WirePlumber Backend

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Introduction

WirePlumber is the session manager used by PipeWire.

Project Atlas does not communicate directly with PipeWire for policy
decisions.

Instead, routing policies are delegated to WirePlumber whenever possible.

This document explains how Audio Profile Manager integrates with
WirePlumber.

-------------------------------------------------------------------------------

# Responsibilities

WirePlumber is responsible for:

- Default devices
- Stream routing
- Metadata
- Policy decisions
- Automatic reconnection
- Session management

Project Atlas does not attempt to replace these responsibilities.

Instead, it cooperates with WirePlumber.

-------------------------------------------------------------------------------

# Philosophy

Project Atlas should request policy changes.

WirePlumber should execute those changes.

Example:

Project Atlas

↓

Set default sink

↓

WirePlumber

↓

Policy Engine

↓

PipeWire Graph

This keeps the system compatible with future WirePlumber versions.

-------------------------------------------------------------------------------

# Metadata

WirePlumber stores dynamic configuration using Metadata objects.

Examples:

default.configured.audio.sink

default.configured.audio.source

target.object

Metadata represents user intent rather than low-level graph changes.

-------------------------------------------------------------------------------

# target.object

One of the most important metadata keys.

Purpose:

Move an existing stream to another destination.

Current workflow:

Application Stream

↓

target.object

↓

WirePlumber

↓

Routing Policy

↓

New Sink

This avoids manipulating PipeWire links manually.

-------------------------------------------------------------------------------

# Default Devices

Current implementation:

audio-profile default-sink <sink>

↓

wpctl set-default

↓

WirePlumber

↓

Metadata

↓

Policy update

The same approach is used for:

audio-profile default-source <source>

-------------------------------------------------------------------------------

# Current Backend

Today the backend uses:

wpctl

pw-metadata

through the Exec backend.

Future implementation:

Native WirePlumber API.

-------------------------------------------------------------------------------

# Native Goals

Future backend responsibilities:

- Metadata access.
- Object Manager.
- Default devices.
- Stream movement.
- Policy interaction.

Without external commands.

-------------------------------------------------------------------------------

# WpObjectManager

Future versions should use:

WpObjectManager

to discover:

Devices

Nodes

Metadata

Default objects

Policy objects

This removes the dependency on command-line parsing.


-------------------------------------------------------------------------------

# WpMetadata

WirePlumber exposes metadata through WpMetadata objects.

Metadata is one of the most important concepts used by
Project Atlas.

Examples of metadata:

default.configured.audio.sink

default.configured.audio.source

target.object

Future versions may expose additional metadata keys.

-------------------------------------------------------------------------------

# Object Manager

The Object Manager is responsible for tracking
WirePlumber objects.

Future backend responsibilities:

• Discover metadata objects.

• Track default devices.

• Track routing changes.

• Detect metadata updates.

The Core should never manipulate Object Manager directly.

-------------------------------------------------------------------------------

# Event Driven Design

One long-term goal is to move away from polling.

Instead of repeatedly asking:

"What is the default sink?"

Project Atlas should receive notifications when it changes.

Example:

WirePlumber

↓

Metadata changed

↓

Backend

↓

Core

↓

Registry Cache

↓

Doctor

↓

TUI

-------------------------------------------------------------------------------

# Registry Synchronization

Future versions will synchronize PipeWire and
WirePlumber into a single Registry.

PipeWire provides:

Devices

Nodes

Ports

Links

Streams

WirePlumber provides:

Metadata

Policies

Defaults

Routing decisions

Together they produce the complete Project Atlas model.

-------------------------------------------------------------------------------

# Stream Routing

Current implementation:

Project Atlas

↓

pw-metadata

↓

target.object

↓

WirePlumber

↓

Routing

Future implementation:

Project Atlas

↓

Native WirePlumber API

↓

Metadata

↓

Routing

The user interface should not notice the difference.

-------------------------------------------------------------------------------

# Why Separate PipeWire and WirePlumber?

Although they work together, they solve different problems.

PipeWire:

• Audio graph

• Devices

• Nodes

• Ports

• Links

• Streams

WirePlumber:

• Policies

• Metadata

• Defaults

• Automatic routing

Keeping separate backends allows each subsystem to evolve
independently.

-------------------------------------------------------------------------------

# Future Native Backend

Current implementation uses command-line tools.

Future implementation should communicate directly with:

libwireplumber

Advantages:

• Faster execution.

• Better error handling.

• Live updates.

• Structured objects.

-------------------------------------------------------------------------------

# Automation Integration

Automation rules will use WirePlumber events.

Example:

When:

Default sink changes

Then:

Update Registry

Run Doctor

Refresh TUI

Another example:

When:

SPCPlay starts

Then:

Assign Retro Workspace

Move stream

Load recommended profile

-------------------------------------------------------------------------------

# Error Handling

The WirePlumber backend should always return structured
errors.

Examples:

- Metadata unavailable

- Default sink not found

- Invalid object

- Permission denied

The Core should decide how to present these errors.

-------------------------------------------------------------------------------

# Long-Term Goal

WirePlumber should become an implementation detail.

Project Atlas should expose a stable Core API that hides
backend complexity.

Applications built on top of the Core should never need to
know whether information came from:

PipeWire

WirePlumber

ALSA

or another backend.

This abstraction is one of the key design principles of
Project Atlas.

