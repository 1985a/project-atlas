# Audio Profile Manager

# ALSA Backend

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Introduction

This document describes how Project Atlas interacts with ALSA.

It explains:

- Mixer controls.
- Hardware detection.
- ALSA profiles.
- Profile restoration.
- Future native ALSA backend.

-------------------------------------------------------------------------------

# Why ALSA

ALSA is the lowest audio layer used by Project Atlas.

It provides:

• Hardware devices

• Mixer controls

• PCM devices

• Control interfaces

Every sound card supported by Linux exposes ALSA controls.

-------------------------------------------------------------------------------

# Current Implementation

Today Project Atlas uses:

amixer

alsactl

through the Exec backend.

Current commands:

audio-profile save

audio-profile load

audio-profile ca0132-optimal

Future versions should progressively migrate to the native ALSA API.

-------------------------------------------------------------------------------

# Mixer Controls

The ALSA backend models every useful control as a
MixerControl object.

Example controls:

Master Playback Volume

PCM Playback Volume

Front Playback Volume

Surround Playback Volume

Center Playback Volume

LFE Playback Volume

Bass Redirection

Equalizer Bands

Capture Gain

Microphone Boost

-------------------------------------------------------------------------------

# MixerControl Object

Each control stores:

Name

Type

Readable

Writable

Minimum value

Maximum value

Current value

Recommended value

Future versions will manipulate MixerControl objects instead
of parsing command output.

-------------------------------------------------------------------------------

# Current Save Process

Current implementation:

audio-profile save music

↓

alsactl store

↓

Store asound.state

↓

Save PipeWire information

↓

Save metadata

↓

Save graph

This provides a complete snapshot.

-------------------------------------------------------------------------------

# Current Load Process

Current implementation:

audio-profile load music

↓

alsactl restore

↓

Restore metadata

↓

Restore defaults

↓

Restore routing

This works but still depends on the complete ALSA state.

-------------------------------------------------------------------------------

# Problem with Full ALSA Restore

A complete asound.state contains every ALSA control.

Some hardware drivers expose controls that:

• Are temporary.

• Are read-only.

• Change across driver versions.

• Reject writes.

Example observed on CA0132:

"Wedge Angle Capture Volume"

The driver rejects writing this control.

The restore process continues, but warnings are generated.


Update: 07/25/26
The app writes well now.

CA0132

Si alsactl restore informa:

"Wedge Angle Capture Volume"

comprobar que el control numid=44
contiene un valor válido.

Rango:

20..180

El preset puede regenerarse mediante:

alsactl --file presets/alsa/ca0132-current.state store Creative
-------------------------------------------------------------------------------

# Design Decision

Project Atlas will stop using complete asound.state
as the primary profile format.

Instead, profiles will store only useful MixerControl
objects.

This makes restoration:

• Faster.

• Safer.

• Easier to validate.

• Hardware-aware.


-------------------------------------------------------------------------------

# Future Mixer Format

Project Atlas will introduce a hardware-independent mixer format.

Instead of storing:

asound.state

The project will store:

MixerControl objects

↓

JSON

Example:

{
    "Master Playback Volume": {
        "type": "integer",
        "values": [70,70]
    },

    "PCM Playback Volume": {
        "values": [237,237]
    },

    "Front Playback Volume": {
        "values": [95,95]
    }
}

Only meaningful controls are stored.

-------------------------------------------------------------------------------

# controls.json

Every supported hardware may contain:

hardware/

    <device>/

        controls.json

This file defines:

• Writable controls.

• Recommended values.

• Safe restoration order.

• Optional validation rules.

-------------------------------------------------------------------------------

# Export

Future command:

audio-profile alsa-export

Workflow:

ALSA

↓

Mixer Controls

↓

MixerControl Objects

↓

controls.json

This export becomes the recommended mixer snapshot.

-------------------------------------------------------------------------------

# Import

Future command:

audio-profile alsa-import

Workflow:

controls.json

↓

MixerControl Objects

↓

ALSA Backend

↓

Hardware

Only supported controls are restored.

Unknown controls are ignored.

-------------------------------------------------------------------------------

# Validation

Before restoring a control,
Project Atlas should verify:

• Control exists.

• Control is writable.

• Value is inside valid range.

• Hardware supports it.

Invalid controls are skipped and reported.

-------------------------------------------------------------------------------

# CA0132 Support

The Creative Sound Blaster Z (CA0132) is the first
hardware platform officially supported.

Known characteristics:

• Hardware equalizer

• Bass Redirection

• Multiple playback channels

• Driver-specific controls

• Some controls reject writes through alsactl

Project Atlas should encapsulate these behaviors in
the CA0132 hardware driver.

-------------------------------------------------------------------------------

# Safe Restore

Future restoration process:

Profile

↓

MixerControl Objects

↓

Validate

↓

Restore only valid controls

↓

Verify result

↓

Generate report

This avoids restoring unnecessary or unsupported
hardware state.

-------------------------------------------------------------------------------

# Doctor Integration

The Doctor subsystem compares:

Current MixerControl values

↓

Recommended values

↓

Profile values

Possible diagnostics:

• Volume mismatch

• Muted output

• Invalid balance

• Missing controls

• Driver-specific warnings

-------------------------------------------------------------------------------

# Repair Integration

Repair should modify only the controls that differ
from the selected profile.

Example:

Current:

Master = 55

Recommended:

Master = 70

Repair changes only:

Master

Everything else remains untouched.

-------------------------------------------------------------------------------

# Hardware Recommendations

The Hardware DB may include:

recommendations.json

Example:

{
    "Master": 70,
    "PCM": 237,
    "Front": 95,
    "Rear": 95,
    "Center": 95,
    "LFE": 95
}

Doctor and Repair use these values when no user
profile exists.

-------------------------------------------------------------------------------

# Native ALSA Backend

Current implementation:

amixer

alsactl

Future implementation:

libasound

Advantages:

• Faster

• No command parsing

• Better error reporting

• Direct control access

• Cleaner architecture

-------------------------------------------------------------------------------

# Long-Term Goal

The ALSA backend should become responsible only for:

• Discovering mixer controls.

• Reading values.

• Writing values.

• Reporting hardware capabilities.

Everything else belongs to the Core.

The Core owns the logic.

The ALSA backend owns the communication.

This separation is one of the central principles of
Project Atlas.

