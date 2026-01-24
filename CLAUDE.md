# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Ruby gem that provides bindings to libdmtx for encoding Data Matrix barcodes. It's a C extension gem that wraps libdmtx and uses libpng for fast PNG output, avoiding any dependency on ImageMagick/RMagick.

## System Dependencies

Runtime libraries:
- libdmtx (Data Matrix encoding)
- libpng (PNG writer)

Build dependencies:
- libdmtx-dev
- libpng-dev
- build-base/build-essential

Ubuntu/Debian install:
```bash
sudo apt-get install libdmtx-dev libpng-dev
```

Alpine install:
```bash
apk add --no-cache build-base libdmtx-dev libpng-dev
```

## Build and Test Commands

Build the native extension:
```bash
cd ext/rdmtx
ruby extconf.rb
make
cd ../..
```

Run tests:
```bash
rake test
```

Build gem:
```bash
gem build Rdmtx.gemspec
```

Install local gem for testing:
```bash
gem install ./Rdmtx-0.5.2.gem
```

## Architecture

### C Extension Layer (ext/rdmtx/Rdmtx.c)

The core of the gem is a C extension that bridges Ruby and libdmtx:

- **Rdmtx class**: Main wrapper class with an `encode` method
- **Rdmtx::Image class**: Wrapper around PNG binary data with `write(path)` and `data` methods
- **PNG generation**: Uses libpng directly via a custom write callback (`png_write_callback`) that writes PNG data into a Ruby string instead of to disk
- **Global constants**: Exposes all DmtxSymbol size constants (DmtxSymbolSquareAuto, DmtxSymbol10x10, etc.)

Key implementation details:
- Row stride calculation at Rdmtx.c:157 is critical for proper PNG generation
- PNG is written in-memory to a Ruby string, not to a file
- The `rdmtx_encode` function handles optional arguments with defaults (margin=5, module=5, size=DmtxSymbolSquareAuto)

### Ruby API

```ruby
rdmtx = Rdmtx.new
image = rdmtx.encode(string, margin_size, module_size, size_request)
image.write("output.png")  # Writes PNG to disk
png_data = image.data       # Returns PNG as binary string
```

### Test Structure

Tests use minitest and include:
- PngHelpers module in test/test_helper.rb for PNG validation
- Tests verify PNG signature, dimensions, and proper encoding
- Tests require the extension to be built first

## Important Notes

- This fork removes the `decode` functionality from the original gem
- PNG generation happens entirely in C code for performance
- The gem version is defined in Rdmtx.gemspec (currently 0.5.2)
- CI runs on GitHub Actions with Ruby 3.2

## Code Quality and Security (Updated 2026-01-24)

The C extension has been thoroughly reviewed and hardened for production use:

### Security Features
- **Memory safety**: rb_ensure() guarantees resource cleanup on all code paths
- **Input validation**: All parameters validated with safe limits (MAX_MARGIN=1000, MAX_MODULE=100)
- **Overflow protection**: check_mul_overflow() prevents integer overflow attacks
- **NULL checks**: All allocations checked before use

### Performance Optimizations
- Cached method IDs (id_binwrite) to avoid repeated rb_intern() calls
- Pre-allocated PNG strings to minimize realloc operations
- Average encode time: ~0.35ms (excellent performance)

### Testing
Run extended tests to verify all fixes:
```bash
ruby test_fixes.rb
```

### Known Constraints
- Maximum margin: 1000 pixels
- Maximum module size: 100 pixels
- Maximum image dimension: 10000 pixels
- These limits prevent DoS attacks and ensure stable operation

See CODE_REVIEW.md, FIXES_EXPLAINED.md, and FIXES_APPLIED.md for detailed information about security improvements.
