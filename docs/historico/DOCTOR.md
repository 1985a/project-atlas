# Audio Profile Manager

# Doctor Engine

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Introduction

Doctor is the diagnostic engine of Project Atlas.

Its purpose is to analyze the complete Linux audio environment and
detect configuration problems, inconsistencies and deviations from the
expected profile.

Doctor never modifies the system.

It only analyzes and reports.

-------------------------------------------------------------------------------

# Objectives

Doctor should answer questions such as:

• Is the correct hardware being used?

• Are the mixer controls correct?

• Is the default sink valid?

• Are streams routed correctly?

• Are metadata values correct?

• Is the current profile active?

• Are there broken PipeWire links?

• Are recommended values being used?

-------------------------------------------------------------------------------

# Architecture

Current System

↓

Registry

↓

Hardware DB

↓

Profile

↓

Workspace

↓

Doctor

↓

Report

Doctor always compares reality against the expected state.

-------------------------------------------------------------------------------

# Diagnostic Sources

Doctor collects information from:

• Hardware Database

• ALSA Backend

• PipeWire Backend

• WirePlumber Backend

• Current Profile

• Active Workspace

-------------------------------------------------------------------------------

# Diagnostic Categories

Doctor is divided into modules.

Hardware

Mixer

PipeWire

WirePlumber

Routing

Streams

Effects

Profiles

Workspace

Automation

Each module generates independent results.

-------------------------------------------------------------------------------

# Hardware Module

Checks:

• Supported hardware

• Hardware identifier

• Driver detection

• Hardware capabilities

• Hardware profile

Example:

✔ Creative Sound Blaster Z detected

Chip:

CA0132

Driver:

snd_ca0132

-------------------------------------------------------------------------------

# Mixer Module

Checks:

• Missing controls

• Read-only controls

• Invalid values

• Recommended values

• Hardware limits

Example:

Master Playback Volume

Expected:

70

Current:

55

Status:

Warning

-------------------------------------------------------------------------------

# PipeWire Module

Checks:

• Registry available

• Devices

• Nodes

• Ports

• Links

• Streams

• Graph consistency

Example:

✔ Registry

✔ Nodes

✔ Links

⚠ Missing playback stream

-------------------------------------------------------------------------------

# WirePlumber Module

Checks:

• Metadata

• Default sink

• Default source

• target.object

• Policy availability

-------------------------------------------------------------------------------

# Routing Module

Checks:

• Stream destinations

• Missing links

• Invalid routing

• Unexpected routing

-------------------------------------------------------------------------------

# Stream Module

Checks:

• Active streams

• Missing streams

• Duplicate streams

• Muted streams

• Invalid sample rates

• Invalid channel count

-------------------------------------------------------------------------------

# Effect Module

Future support:

EasyEffects

Carla

PipeWire Filter Chains

Checks:

• Effect engine running

• Selected preset

• Missing preset

• Disabled processing


-------------------------------------------------------------------------------

# Health Score

Doctor computes an overall system health score.

Range:

0

↓

100

Meaning:

100

Perfect configuration

90-99

Minor recommendations

75-89

Warnings

50-74

Configuration problems

Below 50

Repair strongly recommended

-------------------------------------------------------------------------------

# Severity Levels

Doctor classifies every finding.

INFO

Normal information.

WARNING

Configuration differs from recommendation.

ERROR

Configuration prevents expected operation.

FATAL

Subsystem unavailable.

-------------------------------------------------------------------------------

# Recommendation Engine

Every diagnostic may include recommendations.

Example:

Current:

Master Playback Volume = 55

Recommended:

70

Recommendation:

Increase Master Playback Volume to 70.

-------------------------------------------------------------------------------

# Repair Suggestions

Doctor never modifies the system.

Instead it may suggest:

audio-profile repair

or

audio-profile repair music

The user decides whether repairs should be applied.

-------------------------------------------------------------------------------

# Output Formats

Doctor should support multiple formats.

Human readable:

audio-profile doctor

Machine readable:

audio-profile doctor --json

Future:

audio-profile doctor --html

audio-profile doctor --markdown

-------------------------------------------------------------------------------

# Example Output

Hardware

✔ Creative Sound Blaster Z

Driver

✔ snd_ca0132

Mixer

⚠ Master Playback Volume differs

⚠ PCM differs

PipeWire

✔ Registry

✔ Devices

✔ Nodes

✔ Links

WirePlumber

✔ Metadata

✔ Defaults

Routing

✔ Streams correctly assigned

Effects

✔ EasyEffects preset loaded

Overall Health

97 / 100

-------------------------------------------------------------------------------

# JSON Output

Example:

{
    "health":97,

    "hardware":"ca0132",

    "warnings":[

        "Master Playback Volume differs",

        "PCM differs"

    ],

    "errors":[]
}

-------------------------------------------------------------------------------

# Doctor API

Future Core API:

ppm_doctor_run()

ppm_doctor_report()

ppm_doctor_export_json()

ppm_doctor_export_text()

Each diagnostic module contributes to the final report.

-------------------------------------------------------------------------------

# Integration with Repair

Doctor and Repair are complementary.

Doctor:

Analyze

↓

Compare

↓

Report

Repair:

Analyze

↓

Modify only differences

↓

Verify

Doctor should always run before Repair.

-------------------------------------------------------------------------------

# Future Modules

Future diagnostic modules may include:

Bluetooth

HDMI Audio

USB Audio

JACK Bridges

EasyEffects

Carla

PipeWire Filter Chains

Network Audio

-------------------------------------------------------------------------------

# Long-Term Goal

Doctor should become the primary diagnostic tool for
Project Atlas.

Its mission is to explain:

• What is wrong.

• Why it is wrong.

• How severe it is.

• How to fix it.

without modifying the system automatically.

The Core owns the diagnostic logic.

Backends only provide the required information.

