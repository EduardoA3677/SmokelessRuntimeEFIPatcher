# Contributing to SmokelessRuntimeEFIPatcher

Thank you for your interest in contributing to SREP! This document provides guidelines for contributing to the project.

## Code of Conduct

- Be respectful and constructive
- Focus on the technical merits
- Help others learn and grow

## How to Contribute

### Reporting Bugs

When reporting bugs, please include:
- Your BIOS type and version
- Steps to reproduce
- Expected vs actual behavior
- Any error messages or logs (SREP.log)
- Hardware information

### Suggesting Features

Feature suggestions are welcome! Please:
- Explain the use case
- Describe the desired behavior
- Consider compatibility with different BIOS types

### Pull Requests

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Test thoroughly on real hardware
5. Commit with clear messages
6. Push to your fork
7. Open a Pull Request

### Code Style

- Follow existing code style
- Use meaningful variable names
- Add comments for complex logic
- Document public functions
- Keep functions focused and small
- Handle errors appropriately

### Testing

- Test on real UEFI hardware when possible
- Test in QEMU/Virtual machines
- Verify no regressions in existing functionality
- Test error paths and edge cases

## Development Setup

### Prerequisites

- Linux development environment
- EDK2 build environment
- GCC 11 or later
- NASM assembler
- Python 3

### Building

See README.md for detailed build instructions.

### Debugging

- Use UEFI Shell for testing
- Check SREP.log for detailed logs
- Use DEBUG builds for more information
- Test on multiple BIOS types

## What We're Looking For

- Bug fixes
- Performance improvements
- Support for additional BIOS types
- Better error messages
- Documentation improvements
- Code cleanup and refactoring

## What to Avoid

- Breaking changes without discussion
- Adding unnecessary dependencies
- Removing features without deprecation
- Style-only changes
- Uncommitted binary files

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

## Questions?

Open an issue for questions or discussions!
