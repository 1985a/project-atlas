# Audio Profile Manager

# Repair Engine

Version: 1.0

Status: Frozen

-------------------------------------------------------------------------------

# Introduction

Repair is the corrective engine of Project Atlas.

Unlike Doctor, Repair modifies the system.

Its objective is to restore only the components that differ from
the expected configuration.

Repair must never overwrite the entire audio configuration unless
explicitly requested.

-------------------------------------------------------------------------------

# Design Philosophy

Repair follows a conservative strategy.

Current System

↓

Doctor

↓

Differences

↓

Repair

↓

Verification

↓

Report

Only modified values should be restored.

-------------------------------------------------------------------------------

# Objectives

Repair should:

• Restore missing mixer values.

• Restore routing.

• Restore metadata.

• Restore default devices.

• Restore selected streams.

• Restore hardware recommendations.

Without modifying unrelated configuration.

-------------------------------------------------------------------------------

# Relationship with Doctor

Doctor detects problems.

Repair solves problems.

Doctor

↓

Report

↓

Repair

↓

Verification

↓

Doctor

The final Doctor execution confirms that the repair succeeded.

-------------------------------------------------------------------------------

# Repair Sources

Repair receives information from:

• Hardware Database

• Current Profile

• Active Workspace

• Current Registry

• ALSA Backend

• PipeWire Backend

• WirePlumber Backend

-------------------------------------------------------------------------------

# Repair Categories

Repair is divided into modules.

Hardware

Mixer

PipeWire

WirePlumber

Routing

Streams

Effects

Workspace

Automation

Each module restores only its own responsibilities.

-------------------------------------------------------------------------------

# Hardware Repair

Responsibilities:

• Detect supported hardware.

• Verify hardware identifier.

• Verify capabilities.

• Ignore unsupported hardware.

Hardware replacement is never automatic.

-------------------------------------------------------------------------------

# Mixer Repair

Checks:

Current value

↓

Recommended value

↓

Difference

↓

Apply only if necessary

Example:

Master

Current:

55

Recommended:

70

Repair changes:

Master only.

-------------------------------------------------------------------------------

# Routing Repair

Checks:

Expected routing

↓

Current routing

↓

Missing links

↓

Restore routing

Routing repair never recreates the entire graph.

Only missing or incorrect routes are updated.

-------------------------------------------------------------------------------

# Metadata Repair

Checks:

Default sink

Default source

target.object

Workspace metadata

Only incorrect values are updated.


-------------------------------------------------------------------------------

# Stream Repair

The Stream module restores application routing.

Workflow:

Current Streams

↓

Expected Streams

↓

Difference

↓

Move only incorrect streams

Example:

Firefox

Current:

HDMI

Expected:

CA0132

Repair:

Move Firefox only.

-------------------------------------------------------------------------------

# Profile Repair

Profiles are compared against the current system.

Repair may restore:

• Mixer values

• Routing

• Metadata

• Default devices

• Stream targets

Profiles are restored selectively.

-------------------------------------------------------------------------------

# Workspace Repair

Workspace Repair restores the operational environment.

Example:

Workspace

Gaming

↓

Profile

Gaming

↓

Effects

Gaming

↓

Routing

Gaming

↓

Automation

Gaming

Only components belonging to the active Workspace
are modified.

-------------------------------------------------------------------------------

# Effect Repair

Future support:

EasyEffects

Carla

PipeWire Filter Chains

Possible repairs:

• Missing preset

• Disabled processing

• Wrong preset

• Missing plugin

-------------------------------------------------------------------------------

# Verification

Every repair operation should be verified.

Repair

↓

Read Current State

↓

Compare

↓

Success?

↓

Generate Report

No repair is considered complete until verification succeeds.

-------------------------------------------------------------------------------

# Safe Mode

Future command:

audio-profile repair --safe

Safe mode restores only:

• Mixer controls

• Metadata

• Defaults

No routing changes are performed.

-------------------------------------------------------------------------------

# Dry Run

Future command:

audio-profile repair --dry-run

Doctor computes:

Current

↓

Expected

↓

Differences

↓

Repair Plan

↓

Display Only

No changes are made.

-------------------------------------------------------------------------------

# Interactive Repair

Future command:

audio-profile repair --interactive

Example:

Master Playback Volume

Current:

55

Recommended:

70

Apply?

[Y] Yes

[N] No

[A] All

[S] Skip

-------------------------------------------------------------------------------

# Automatic Repair

Future command:

audio-profile repair music

Workflow:

Doctor

↓

Repair Plan

↓

Apply

↓

Verify

↓

Doctor

↓

Final Report

-------------------------------------------------------------------------------

# JSON Report

Future output:

{
    "repaired": [

        "Master Playback Volume",

        "Default Sink",

        "Firefox Routing"

    ],

    "skipped": [

        "Unsupported Mixer Control"

    ],

    "failed": [

    ]
}

-------------------------------------------------------------------------------

# Repair API

Future Core API:

ppm_repair_run()

ppm_repair_plan()

ppm_repair_apply()

ppm_repair_verify()

ppm_repair_export_json()

-------------------------------------------------------------------------------

# Error Recovery

If a repair operation fails:

• Stop the current module.

• Continue independent modules when possible.

• Record failure.

• Report partial success.

Repair should never leave the system in an unknown state.

-------------------------------------------------------------------------------

# Backup Strategy

Before modifying the system,
Repair may create a temporary backup.

Future workflow:

Backup

↓

Repair

↓

Verification

↓

Success

↓

Delete Backup

Failure

↓

Restore Backup

-------------------------------------------------------------------------------

# Long-Term Goal

Repair should become one of the safest Linux audio
recovery systems.

Its philosophy is simple:

Never overwrite everything.

Never modify unrelated configuration.

Always verify.

Always explain.

Always remain reversible.

Doctor diagnoses.

Repair restores.

Together they provide a complete maintenance
framework for Project Atlas.

