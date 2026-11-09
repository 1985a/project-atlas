# Audio Profile Manager

# PipeWire Backend

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Introduction

This document explains how Audio Profile Manager interacts with PipeWire.

It is not intended to replace the official PipeWire documentation.

Instead, it describes:

- the internal architecture,
- the object model,
- the Registry,
- and how Project Atlas uses PipeWire internally.

-------------------------------------------------------------------------------

# Why PipeWire

PipeWire provides a modern graph-based audio engine.

Unlike PulseAudio, PipeWire exposes a complete object model.

Everything inside PipeWire is represented as objects.

Examples:

Device

Node

Port

Link

Client

Metadata

This matches perfectly with Project Atlas.

-------------------------------------------------------------------------------

# PipeWire Registry

The Registry is the entry point.

Everything begins here.

PipeWire

↓

Registry

↓

Objects

↓

Core Objects

↓

Audio Profile Manager

The Registry is scanned to discover every object.

-------------------------------------------------------------------------------

# Registry Objects

Current object types used by Project Atlas:

Device

Represents physical or virtual hardware.

Examples:

Creative Sound Blaster Z

USB DAC

HDMI Audio

-------------------------------------------------------------------------------

Node

Represents an endpoint.

Examples:

Playback node

Capture node

Virtual sink

Virtual microphone

Application stream

Every stream is a Node.

-------------------------------------------------------------------------------

Port

Represents one audio channel endpoint.

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

Represents one connection.

Output Port

↓

Input Port

Links define routing.

-------------------------------------------------------------------------------

Client

Represents one application connected to PipeWire.

Examples:

Firefox

Discord

MPD

SPCPlay

RetroArch

-------------------------------------------------------------------------------

Metadata

Metadata stores dynamic configuration.

Examples:

Default sink

Default source

target.object

Routing hints

WirePlumber relies heavily on Metadata.

-------------------------------------------------------------------------------

Registry Scan

Current implementation:

Connect

↓

Registry

↓

Collect objects

↓

Store temporary objects

↓

Print

Future implementation:

Connect

↓

Registry Cache

↓

Core Objects

↓

Shared by all modules

-------------------------------------------------------------------------------

Current Commands

The following commands already use the Registry:

audio-profile devices

audio-profile sinks

audio-profile sources

audio-profile streams

audio-profile ports

audio-profile links

audio-profile graph

Each command currently performs its own Registry scan.

Future versions will use a shared Registry Cache.

-------------------------------------------------------------------------------

Registry Cache

Future design:

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

Profiles

↓

Doctor

↓

Repair

↓

TUI

The Registry should be scanned only once.


-------------------------------------------------------------------------------

# Building the Graph

One of the first goals of Project Atlas was to understand the complete
PipeWire routing graph.

The graph is composed of four primary object types:

Device

↓

Node

↓

Port

↓

Link

Streams are represented as Nodes.

-------------------------------------------------------------------------------

# Relationship Model

Hardware

↓

Device

↓

Playback Node

↓

Output Ports

↓

Links

↓

Input Ports

↓

Application Stream

↓

Client

Every routing decision can be reconstructed from these relationships.

-------------------------------------------------------------------------------

# Remote Port Resolution

A Link only stores identifiers.

Example:

Output Port ID

↓

Input Port ID

Project Atlas resolves both identifiers into real objects.

Result:

Output Node

↓

Output Port

↓

Input Port

↓

Input Node

This allows graph visualization.

-------------------------------------------------------------------------------

# Remote Node Resolution

After resolving Ports,
Project Atlas resolves the owning Nodes.

The resulting graph becomes:

Node

↓

Port

↓

Link

↓

Port

↓

Node

This structure is independent of PipeWire commands.

-------------------------------------------------------------------------------

# Stream Detection

Application streams are detected using Node properties.

Typical media classes:

Audio/Sink

Audio/Source

Stream/Output/Audio

Stream/Input/Audio

Additional information:

Application name

Node name

Client ID

Media class

Target device

These values populate PpmStream objects.

-------------------------------------------------------------------------------

# Moving Streams

Moving streams was one of the most important milestones.

Instead of reconnecting Links directly,
Project Atlas relies on WirePlumber metadata.

Current implementation:

target.object

↓

WirePlumber

↓

Policy Engine

↓

Move Stream

This approach is safer than manipulating PipeWire links manually.

-------------------------------------------------------------------------------

# Why not use pw-link?

Creating or removing Links manually can interfere with
WirePlumber's policy decisions.

WirePlumber is responsible for maintaining routing.

Therefore:

Project Atlas requests routing changes.

WirePlumber performs the routing.

This preserves compatibility with future policy changes.

-------------------------------------------------------------------------------

# Metadata

Metadata is used for:

Default sink

Default source

Stream target

Future routing hints

Current implementation uses:

pw-metadata

Future implementation:

Native WirePlumber Metadata API.

-------------------------------------------------------------------------------

# Graph Rendering

Current graph output is generated by traversing:

Registry

↓

Nodes

↓

Ports

↓

Links

↓

Remote Nodes

Future versions may support:

Tree view

Dependency graph

GraphViz export

JSON export

Interactive navigation

-------------------------------------------------------------------------------

# Registry Cache

Current behavior:

Every command scans the Registry independently.

Future behavior:

PipeWire

↓

Registry

↓

PpmRegistry

↓

Shared Objects

↓

All Modules

Advantages:

- Faster execution.
- Consistent state.
- Simpler code.
- Easier diagnostics.
- Better TUI performance.

-------------------------------------------------------------------------------

# Backend Responsibilities

The PipeWire backend should only:

- Discover objects.
- Update Registry.
- Convert objects.
- Report events.

It should never:

- Store profiles.
- Implement diagnostics.
- Contain hardware-specific logic.

Those responsibilities belong to the Core.

-------------------------------------------------------------------------------

# Long-Term Goal

The PipeWire backend should eventually become almost invisible.

Its only purpose is to translate PipeWire objects into
Project Atlas objects.

Everything else should operate entirely on the internal model.

