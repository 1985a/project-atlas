# Hardware Database

Version: 1.0

Status: Frozen Design

-------------------------------------------------------------------------------

# Purpose

The Hardware Database (Hardware DB) is one of the core concepts of Project Atlas.

Instead of treating every sound card as a generic ALSA device,
Audio Profile Manager stores structured information about each supported
hardware platform.

The Hardware DB allows the application to:

- Recognize audio hardware.
- Know hardware capabilities.
- Store recommended mixer settings.
- Track known driver issues.
- Build hardware-specific profiles.
- Provide diagnostics.
- Apply hardware-specific repairs.

-------------------------------------------------------------------------------

# Directory Layout

hardware/

    ca0132/

        card.json

        controls.json

        capabilities.json

        recommendations.json

        ports.json

        nodes.json

        known_bugs.json

        profiles/

            music.json

            gaming.json

            movie.json

            optimal.json

Every supported hardware platform follows the same layout.

-------------------------------------------------------------------------------

# card.json

Describes the physical hardware.

Example:

{
    "id": "ca0132",
    "name": "Creative Sound Blaster Z",
    "chip": "CA0132",
    "model": "SB1570",
    "manufacturer": "Creative",
    "alsa_card": 0
}

-------------------------------------------------------------------------------

# controls.json

Contains only the ALSA controls considered useful by Audio Profile Manager.

Unlike a complete `asound.state`, this file intentionally ignores
temporary or problematic controls.

Example:

{
    "Master Playback Volume": {
        "type": "integer",
        "recommended": [70,70]
    },

    "PCM Playback Volume": {
        "recommended": [237,237]
    },

    "Front Playback Volume": {
        "recommended": [95,95]
    }
}

-------------------------------------------------------------------------------

# capabilities.json

Capabilities describe what a sound card can do.

Example:

{
    "playback_5_1": true,
    "playback_7_1": false,
    "capture": true,
    "hardware_eq": true,
    "bass_redirection": true,
    "spdif": true,
    "headphone_virtualization": false
}

Capabilities are consumed by the Core.

The Core never needs to know hardware names.

It only checks capabilities.

-------------------------------------------------------------------------------

# recommendations.json

Stores recommended values for a specific hardware platform.

Example:

{
    "speaker": "Logitech X-530",

    "recommended": {

        "Master": 70,

        "PCM": 237,

        "Front": 95,

        "Surround": 95,

        "Center": 95,

        "LFE": 95

    }
}

Recommendations are used by:

- Doctor
- Repair
- Hardware setup
- Workspace initialization


-------------------------------------------------------------------------------

# known_bugs.json

Every hardware platform may contain driver-specific workarounds.

Example:

{
    "driver": "snd_ca0132",

    "known_bugs": [

        {
            "id": "CA0132-0001",

            "description":

            "alsactl restore may fail on Wedge Angle Capture Volume.",

            "severity": "warning",

            "workaround":

            "Ignore this control during profile restoration."
        },

        {
            "id": "CA0132-0002",

            "description":

            "Hardware volume should not be modified through wpctl.",

            "severity": "important",

            "workaround":

            "Modify only application streams or use MixerControl."
        }

    ]
}

-------------------------------------------------------------------------------

# Hardware Profiles

Hardware profiles describe recommended configurations.

Example:

hardware/

    ca0132/

        profiles/

            music.json

            movie.json

            gaming.json

            optimal.json

These profiles are hardware-specific.

They are not user profiles.

-------------------------------------------------------------------------------

# User Profiles

User profiles live outside the Hardware Database.

Location:

~/.config/audio-profile/profiles/

Example:

music/

gaming/

movie/

retro/

streaming/

A user profile references one hardware profile.

-------------------------------------------------------------------------------

# Relationship

Hardware

↓

Hardware Profile

↓

User Profile

↓

Workspace

This separation allows many users to share the same hardware database.

-------------------------------------------------------------------------------

# hardware-scan

One of the most important future commands.

Example:

audio-profile hardware-scan

The scanner will automatically discover:

• ALSA cards

• PipeWire devices

• PipeWire nodes

• PipeWire ports

• PipeWire links

• Streams

• Mixer controls

• Hardware capabilities

-------------------------------------------------------------------------------

# Scan Pipeline

PipeWire Registry

↓

Devices

↓

Nodes

↓

Ports

↓

Links

↓

ALSA

↓

Mixer Controls

↓

Capabilities

↓

Hardware Objects

↓

Hardware DB

-------------------------------------------------------------------------------

# Automatic Generation

The Hardware DB should be generated automatically.

The user should never have to write JSON manually.

Example:

audio-profile hardware-scan

↓

hardware/

    usb-dac/

    ca0132/

    alc1220/

    scarlett2i2/

Every supported hardware follows exactly the same schema.

-------------------------------------------------------------------------------

# Hardware Drivers

Some devices require special logic.

Those devices may have dedicated drivers.

Example:

hardware/

    ca0132/

        driver.c

        driver.h

Optional functions:

ca0132_apply_music()

ca0132_apply_movie()

ca0132_apply_game()

ca0132_doctor()

ca0132_repair()

Hardware drivers encapsulate device-specific knowledge.

-------------------------------------------------------------------------------

# Future Hardware Support

Examples:

Realtek ALC1220

Creative CA0132

Creative AE-5

Creative X4

Focusrite Scarlett

USB DACs

HDMI Audio

Bluetooth Audio

Virtual Devices

PipeWire Virtual Sinks

The Core should support all of them through the same object model.

-------------------------------------------------------------------------------

# Summary

The Hardware Database is not simply a collection of JSON files.

It is a structured knowledge base generated by the system itself.

Its responsibilities include:

• Hardware identification

• Capability discovery

• Mixer recommendations

• Known bugs

• Hardware profiles

• Repair hints

• Diagnostics

• Driver-specific behavior

It becomes the foundation on which Doctor, Repair,
Profiles, Automation and the future TUI are built.

