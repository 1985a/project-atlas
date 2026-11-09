# Audio Profile Manager

# Profile Format Specification

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Purpose

Profiles are the primary persistence format of Project Atlas.

A profile does not simply store volume levels.

A profile represents an entire audio environment.

-------------------------------------------------------------------------------

# Goals

Profiles should be:

• Human readable

• Versioned

• Portable

• Hardware-aware

• Extensible

• Easy to compare

• Easy to validate

-------------------------------------------------------------------------------

# Current Profile

Current implementation stores:

~/.config/audio-profile/profiles/music/

    asound.state

    wpctl-status.txt

    metadata.txt

    links.txt

    pw-dump.json

    profile.json

This provides a complete snapshot.

-------------------------------------------------------------------------------

# Future Profile

The future profile replaces raw command dumps with
structured objects.

Profile

↓

Hardware

↓

Mixer Controls

↓

PipeWire Defaults

↓

Metadata

↓

Streams

↓

Links

↓

Effects

↓

Automation

-------------------------------------------------------------------------------

# Directory Layout

profiles/

    music/

        profile.json

        mixer.json

        routing.json

        metadata.json

        streams.json

        workspace.json

        notes.md

-------------------------------------------------------------------------------

# profile.json

Contains general information.

Example:

{
    "version": 1,

    "name": "Music",

    "description": "High quality music playback",

    "hardware": "ca0132",

    "workspace": "music"
}

-------------------------------------------------------------------------------

# mixer.json

Contains MixerControl objects.

Example:

{
    "Master Playback Volume": [70,70],

    "PCM Playback Volume": [237,237],

    "Front Playback Volume": [95,95]
}

-------------------------------------------------------------------------------

# metadata.json

Stores WirePlumber metadata.

Example:

{
    "default_sink": "alsa_output.ca0132",

    "default_source": "alsa_input.ca0132"
}

-------------------------------------------------------------------------------

# routing.json

Describes stream routing.

Example:

Firefox

↓

HDMI

Discord

↓

USB Headset

MPD

↓

CA0132

Routing is independent from mixer settings.


-------------------------------------------------------------------------------

# streams.json

The streams file describes application-specific routing.

Example:

{
    "streams": [

        {
            "application": "MPD",
            "target": "CA0132",
            "volume": 1.00,
            "mute": false
        },

        {
            "application": "Firefox",
            "target": "HDMI",
            "volume": 0.85,
            "mute": false
        },

        {
            "application": "Discord",
            "target": "USB Headset",
            "volume": 0.90,
            "mute": false
        }

    ]
}

Streams are restored independently from mixer settings.

-------------------------------------------------------------------------------

# workspace.json

A profile may belong to one Workspace.

Example:

{
    "workspace": "music",

    "automation": true,

    "effects": true,

    "routing": true
}

The Workspace determines higher-level behavior.

-------------------------------------------------------------------------------

# effects.json

Future versions may integrate external DSP systems.

Examples:

EasyEffects

Carla

PipeWire Filter Chains

Example:

{
    "engine": "EasyEffects",

    "preset": "Music",

    "enabled": true
}

Project Atlas should activate effects only when available.

-------------------------------------------------------------------------------

# notes.md

Every profile may contain free-form documentation.

Example:

This profile is optimized for:

- Logitech X-530

- Creative Sound Blaster Z

- Music playback

- 48 kHz

- PipeWire

Notes are never interpreted by the Core.

-------------------------------------------------------------------------------

# Profile Versioning

Every profile contains a version number.

Example:

{
    "version": 2
}

Future versions may migrate older profiles automatically.

-------------------------------------------------------------------------------

# Validation

Before loading a profile, Project Atlas should validate:

• Profile version

• Hardware identifier

• Required files

• JSON syntax

• Supported capabilities

• Mixer controls

• Routing entries

Invalid profiles should never be applied silently.

-------------------------------------------------------------------------------

# Hardware Compatibility

Profiles are hardware-aware.

Example:

Hardware:

CA0132

↓

Profile:

Music

↓

Supported

Another example:

Hardware:

USB DAC

↓

CA0132 Profile

↓

Not fully compatible

Doctor should warn the user before loading.

-------------------------------------------------------------------------------

# Profile Migration

Future versions should support migration.

Example:

Profile v1

↓

Migration

↓

Profile v2

Migration keeps old profiles usable after
architecture changes.

-------------------------------------------------------------------------------

# Profile Compare

Future command:

audio-profile profile diff music gaming

Possible output:

Mixer

Master:

70 -> 90

PCM:

237 -> 255

Routing

Firefox

HDMI -> CA0132

Streams

Discord

USB Headset -> Speakers

Effects

EasyEffects preset differs

-------------------------------------------------------------------------------

# Profile Repair

Future command:

audio-profile profile repair music

Workflow:

Profile

↓

Validation

↓

Current System

↓

Difference Detection

↓

Selective Restore

Only modified values should be restored.

-------------------------------------------------------------------------------

# Backup Strategy

Profiles should support automatic backup.

Example:

profiles/

music/

backup/

2026-06-29/

Automatic backups allow safe experimentation.

-------------------------------------------------------------------------------

# Long-Term Goal

Profiles should evolve from simple snapshots into
complete descriptions of an audio environment.

A profile should be capable of restoring:

• Hardware configuration

• Mixer controls

• PipeWire defaults

• WirePlumber metadata

• Stream routing

• Effects

• Automation state

• Workspace association

Profiles become one of the primary objects of
Project Atlas.

