# RFC-0001: Project Atlas

Status: Accepted  
Version: 0.4.0  
Codename: Project Atlas

## Summary

Project Atlas is the frozen architecture for Audio Profile Manager.

The project will evolve from a collection of PipeWire/ALSA control commands into a complete Linux audio management platform.

## Core rule

The Core works with objects.

Backends talk to ALSA, PipeWire and WirePlumber.

The Core must not execute shell commands directly.

## Main objects

- Hardware
- Device
- Node
- Port
- Link
- Stream
- MixerControl
- Profile
- Workspace
- Capability
- Registry

## Milestones

1. Core Refactoring
2. Registry Cache
3. Hardware Scanner
4. Doctor
5. Repair
6. Automation
7. TUI
8. Plugin System
