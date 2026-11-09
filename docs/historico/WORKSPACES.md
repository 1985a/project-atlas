# Audio Profile Manager

# Workspace Specification

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Introduction

A Workspace is the highest-level object in Project Atlas.

A Workspace represents an entire audio environment.

Unlike a Profile, which stores configuration,
a Workspace describes behavior.

-------------------------------------------------------------------------------

# Purpose

A Workspace combines:

• Hardware Profile

• User Profile

• Routing

• Effects

• Automation Rules

• Diagnostics

into a single operational environment.

-------------------------------------------------------------------------------

# Examples

Music

Gaming

Movie

Retro

Streaming

Office

Headphones

Recording

Conference

-------------------------------------------------------------------------------

# Relationship

Workspace

↓

Profile

↓

Hardware

↓

Mixer

↓

PipeWire

↓

WirePlumber

↓

Applications

-------------------------------------------------------------------------------

# Workspace Structure

workspace/

music/

workspace.json

profiles/

rules/

notes.md

-------------------------------------------------------------------------------

# workspace.json

Example:

{
    "version":1,

    "name":"Music",

    "profile":"music",

    "automation":true,

    "effects":true,

    "doctor":true
}

-------------------------------------------------------------------------------

# Responsibilities

A Workspace may define:

Preferred hardware

Preferred routing

Preferred mixer values

Preferred effects

Preferred applications

Preferred automation rules

Preferred diagnostics

-------------------------------------------------------------------------------

# Workspace Activation

Example:

audio-profile workspace music

Workflow:

Load Workspace

↓

Validate Hardware

↓

Load Profile

↓

Restore Mixer

↓

Restore Routing

↓

Restore Metadata

↓

Enable Effects

↓

Enable Automation

↓

Run Doctor

Workspace activation should be safe and repeatable.

-------------------------------------------------------------------------------

# Hardware Awareness

Every Workspace may specify supported hardware.

Example:

Music

↓

Supported:

CA0132

USB DAC

HDMI

Unsupported hardware should generate warnings,
not immediate failures.

-------------------------------------------------------------------------------

# User Overrides

Users may override:

Volumes

Routing

Effects

Defaults

These overrides remain local to the Workspace.


-------------------------------------------------------------------------------

# Automation Rules

A Workspace may define one or more automation rules.

Example:

When:

Application == "SPCPlay"

Then:

Load Profile "retro"

Move Stream -> CA0132

Enable EasyEffects preset "Retro"

-------------------------------------------------------------------------------

# Event Sources

Automation rules may be triggered by:

• Application start

• Application exit

• Hardware connection

• Hardware removal

• Default sink changes

• Default source changes

• PipeWire events

• WirePlumber metadata changes

-------------------------------------------------------------------------------

# Multiple Profiles

A Workspace may contain multiple profiles.

Example:

Workspace

Gaming

↓

Profiles

Headphones

Speakers

Streaming

The active profile is selected automatically
according to the current environment.

-------------------------------------------------------------------------------

# Workspace Priority

If multiple Workspaces match simultaneously,
Project Atlas evaluates priorities.

Example:

Streaming

Priority: 100

Gaming

Priority: 80

Music

Priority: 20

The highest priority Workspace becomes active.

-------------------------------------------------------------------------------

# Workspace Switching

Future command:

audio-profile workspace switch gaming

Workflow:

Validate current hardware

↓

Stop previous automation

↓

Unload previous effects

↓

Load new profile

↓

Restore mixer

↓

Restore routing

↓

Enable effects

↓

Enable automation

↓

Run Doctor

-------------------------------------------------------------------------------

# Effects Integration

Future Workspaces may integrate:

EasyEffects

Carla

PipeWire Filter Chains

LADSPA

LV2

VST (future)

Example:

Workspace

Movie

↓

EasyEffects

Movie preset

↓

Bass enhancement

↓

Limiter

-------------------------------------------------------------------------------

# Doctor Integration

Every Workspace may automatically execute Doctor.

Example:

Workspace activation

↓

Doctor

↓

Health score

↓

Recommendations

↓

Repair (optional)

-------------------------------------------------------------------------------

# Repair Integration

Workspace repair may restore:

• Mixer values

• Routing

• Metadata

• Default sink

• Default source

• Effects

Repair should modify only the values that differ
from the selected Workspace.

-------------------------------------------------------------------------------

# Active Workspace

Only one Workspace should be active at a time.

Future command:

audio-profile workspace current

Example output:

Workspace:

Music

Hardware:

CA0132

Profile:

music

Effects:

EasyEffects Music

Automation:

Enabled

Health:

99/100

-------------------------------------------------------------------------------

# Workspace Persistence

The active Workspace may be stored in:

~/.config/audio-profile/workspace.json

This allows automatic restoration after reboot.

-------------------------------------------------------------------------------

# Workspace API

Future Core API:

ppm_workspace_load()

ppm_workspace_save()

ppm_workspace_activate()

ppm_workspace_deactivate()

ppm_workspace_validate()

ppm_workspace_current()

-------------------------------------------------------------------------------

# Future Extensions

Possible future capabilities:

• Scheduled Workspaces

• Time-based activation

• Location-based activation

• Bluetooth device triggers

• USB DAC auto-selection

• HDMI hotplug activation

• Gaming mode detection

• Streaming mode detection

-------------------------------------------------------------------------------

# Summary

A Workspace is not merely a collection of settings.

It represents the complete operational audio
environment.

It combines:

Hardware

↓

Profiles

↓

Routing

↓

Effects

↓

Automation

↓

Diagnostics

↓

Repair

↓

User preferences

Workspaces become the highest-level object in
Project Atlas and coordinate all lower-level
subsystems through the Core API.

