# Contributing to InKey

Thanks for your interest in contributing to InKey! This is a learning project — we built it to learn C++ and audio DSP — and contributions of all kinds are welcome.

## Getting Started

1. Fork the repository
2. Clone and build following the instructions in [README.md](README.md)
3. Run the tests: `./build/Tests/InKeyTests` (all 114 should pass)

## How to Contribute

### Report Bugs

Open a [GitHub Issue](https://github.com/straplocked/in-key/issues) with:
- What you expected to happen
- What actually happened
- Steps to reproduce
- Your OS, DAW, and build info

### Request Features

Open a [GitHub Issue](https://github.com/straplocked/in-key/issues) and describe the feature you'd like to see. If you have ideas about implementation, feel free to share them.

### Submit Code

1. Create a branch from `master`
2. Make your changes
3. Run the tests and make sure they pass
4. Open a pull request with a clear description of what changed and why

## Code Conventions

- **C++17** standard
- Follow existing patterns in the codebase — JUCE idioms, naming conventions, file organization
- Audio thread code must be lock-free: no allocation, no mutexes, no I/O in `processBlock()`
- New features should include tests where practical (see `Tests/` for examples using Catch2)

## Project Structure

- `Source/` — Plugin source code, organized by subsystem
- `Tests/` — Unit tests (Catch2)
- `Docs/` — Architecture, roadmap, and tech stack documentation

## Questions?

Open an issue — we're happy to help.
