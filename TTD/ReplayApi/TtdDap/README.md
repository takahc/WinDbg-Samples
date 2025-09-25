# TTD Debug Adapter Protocol (TtdDap)

A Debug Adapter Protocol (DAP) server implementation for Time Travel Debugging (TTD) trace files.

## Overview

TtdDap bridges TTD's powerful time-travel debugging capabilities with any DAP-compatible editor or IDE, including Visual Studio Code. This allows developers to debug TTD trace files with familiar debugging interfaces while leveraging TTD's unique ability to navigate backwards and forwards through program execution.

## Features

- **DAP Compliance**: Implements the Debug Adapter Protocol specification for broad compatibility
- **TTD Integration**: Full integration with TTD Replay Engine for trace file analysis
- **Time Travel Navigation**: Support for forward/backward stepping through execution
- **Register Inspection**: View CPU registers and program state at any position
- **Memory Analysis**: Inspect memory contents at different execution points
- **Position Information**: Display current execution position within the trace

## Usage

### Basic Usage

```bash
TtdDap.exe
```

The server communicates via stdin/stdout using the DAP JSON-RPC protocol.

### VS Code Integration

1. Build the TtdDap.exe executable
2. Create a VS Code debug configuration:

```json
{
    "type": "ttd",
    "request": "launch",
    "name": "Debug TTD Trace",
    "program": "path/to/your/trace.ttd"
}
```

3. Install or create a VS Code extension that uses TtdDap.exe as the debug adapter

## Supported DAP Requests

- `initialize` - Initialize the debug adapter
- `launch` - Launch debugging session with a TTD trace file
- `disconnect` - Disconnect from debug session
- `setBreakpoints` - Set breakpoints (placeholder implementation)
- `continue` - Continue execution (moves to end of trace)
- `next` - Step forward one instruction
- `stepIn` - Step into (same as next for TTD)
- `stepOut` - Step out (same as next for TTD)
- `pause` - Pause execution
- `stackTrace` - Get current stack trace
- `scopes` - Get available variable scopes
- `variables` - Get variable values (registers and position info)
- `threads` - Get thread information

## Building

This project requires:
- Visual Studio 2022 or later
- Windows SDK 10.0 or later
- TTD runtime components (automatically downloaded during build)

Build using Visual Studio:
```bash
msbuild TtdDap.sln /p:Configuration=Release /p:Platform=x64
```

Or use the TTD build script from the parent directory:
```bash
cd ..
.\build.cmd
```

## Architecture

TtdDap consists of several key components:

- **TtdDap.cpp**: Main DAP server implementation with JSON-RPC handling
- **DapProtocol.h**: DAP protocol type definitions
- **json.hpp**: Lightweight JSON library for protocol communication

The server integrates with TTD through:
- **TTD Replay Engine**: For loading and analyzing trace files
- **UniqueCursor**: For navigating through trace positions
- **Register Access**: For inspecting CPU state

## Limitations

Current implementation limitations:
- Breakpoints are acknowledged but not functionally implemented
- Single-threaded debugging model
- Limited expression evaluation
- Basic variable inspection (registers and position only)

## Future Enhancements

Planned improvements:
- Full breakpoint support with position-based stopping
- Advanced expression evaluation
- Memory watchpoints integration
- Multi-threaded trace support
- Symbol information integration
- Enhanced variable inspection with memory analysis

## License

Copyright (c) Microsoft Corporation. All rights reserved.
Licensed under the MIT License. See LICENSE in the project root for license information.