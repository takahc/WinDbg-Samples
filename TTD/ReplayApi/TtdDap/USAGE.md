# TTD Debug Adapter Protocol (DAP) Usage Guide

## Building TtdDap

### Prerequisites
- Windows 10 (version 1903 or later) or Windows 11
- Visual Studio 2022 or later with C++ development tools
- Windows SDK 10.0 or later
- Administrative privileges (for TTD operations)

### Build Steps

1. **Download TTD Runtime Components**
   ```cmd
   cd TTD\ReplayApi\GetTtd
   Get-Ttd.cmd
   ```

2. **Build using Visual Studio**
   ```cmd
   cd TTD\ReplayApi\TtdDap
   msbuild TtdDap.sln /p:Configuration=Release /p:Platform=x64
   ```

3. **Or build all TTD samples at once**
   ```cmd
   cd TTD
   build.cmd
   ```

The built executable will be in `TTD\ReplayApi\out\x64-Release\TtdDap.exe`

## Usage

### Command Line Usage

TtdDap is a Debug Adapter Protocol server that communicates via stdin/stdout:

```cmd
TtdDap.exe
```

It expects DAP JSON-RPC messages on stdin and responds on stdout.

### Manual Testing

You can test TtdDap manually by sending JSON messages:

1. **Initialize Request**
   ```json
   Content-Length: 159

   {"seq":1,"type":"request","command":"initialize","arguments":{"clientID":"test","adapterID":"ttd","linesStartAt1":true,"columnsStartAt1":true}}
   ```

2. **Launch Request**
   ```json
   Content-Length: 125

   {"seq":2,"type":"request","command":"launch","arguments":{"program":"C:\\path\\to\\your\\trace.ttd","stopOnEntry":true}}
   ```

3. **Continue/Step Requests**
   ```json
   Content-Length: 81

   {"seq":3,"type":"request","command":"next","arguments":{"threadId":1}}
   ```

### VS Code Integration

See the `example/vscode-extension/` directory for a complete VS Code extension example.

1. **Build the extension**
   ```bash
   cd example/vscode-extension
   npm install
   npm run compile
   ```

2. **Install the extension in VS Code**
   - Package: `vsce package`
   - Install: `code --install-extension ttd-dap-extension-1.0.0.vsix`

3. **Create a debug configuration**
   ```json
   {
       "version": "0.2.0",
       "configurations": [
           {
               "type": "ttd",
               "request": "launch",
               "name": "Debug TTD Trace",
               "program": "${workspaceFolder}/your-trace.ttd",
               "stopOnEntry": true
           }
       ]
   }
   ```

## Supported DAP Features

### Requests
- ✅ `initialize` - Initialize debug adapter
- ✅ `launch` - Launch debugging with TTD trace file
- ✅ `configurationDone` - Configuration complete
- ✅ `disconnect` - Disconnect and exit
- ❌ `attach` - Not supported (TTD uses trace files)
- ✅ `setBreakpoints` - Acknowledged but not functional
- ✅ `continue` - Move to end of trace
- ✅ `next` - Step forward one instruction
- ✅ `stepIn` - Same as next (no function stepping yet)
- ✅ `stepOut` - Same as next (no function stepping yet)
- ✅ `stepBack` - Step backward one instruction (TTD feature!)
- ✅ `pause` - Pause execution
- ✅ `stackTrace` - Get current execution context
- ✅ `scopes` - Get variable scopes (registers)
- ✅ `variables` - Get register and position information
- ✅ `threads` - Get thread information (single thread model)
- ❌ `evaluate` - Expression evaluation not implemented

### Events
- ✅ `initialized` - Adapter initialized
- ✅ `stopped` - Execution stopped (entry, step, pause, end)
- ✅ `terminated` - Debug session terminated

## Key Features

### Time Travel Navigation
The most powerful feature of TtdDap is time-travel debugging:

- **Forward Step**: `next` command moves forward one instruction
- **Backward Step**: `stepBack` command moves backward one instruction
- **Precise Navigation**: Move to any position in the trace
- **Deterministic Replay**: Same execution every time

### Variable Inspection
Current implementation shows:
- Program Counter (instruction pointer)
- Execution Position (sequence:steps format)
- Stack Pointer
- RAX register
- Error information when registers can't be read

### Trace Information
- Trace file path and loading status
- Current position within the trace
- Thread information (simplified single-thread model)

## Limitations and Future Enhancements

### Current Limitations
- **Breakpoints**: Acknowledged but not functionally set
- **Expression Evaluation**: Not implemented
- **Symbol Information**: Not integrated
- **Memory Inspection**: Limited to basic register display
- **Multi-threading**: Single thread model only

### Planned Enhancements
- Position-based breakpoints using TTD's navigation capabilities
- Memory watchpoints integration with TTD's memory analysis
- Symbol resolution for better stack traces and variable names
- Advanced expression evaluation using TTD's memory reading
- Multi-threaded trace support
- Integration with TTD's code coverage and analysis features

## Architecture

TtdDap bridges two worlds:

```
┌─────────────────┐    DAP/JSON-RPC    ┌─────────────────┐
│                 │ ◄─────────────────► │                 │
│   VS Code       │                    │     TtdDap      │
│   (or other     │                    │   Debug Adapter │
│   DAP client)   │                    │                 │
└─────────────────┘                    └─────────────────┘
                                                │
                                                │ TTD Replay API
                                                ▼
                                       ┌─────────────────┐
                                       │   TTD Replay    │
                                       │     Engine      │
                                       │                 │
                                       │  (trace.ttd)    │
                                       └─────────────────┘
```

The adapter translates DAP requests into TTD Replay API calls and forwards TTD state changes back as DAP events.

## Troubleshooting

### Build Issues
- Ensure TTD runtime is downloaded: Run `GetTtd\Get-Ttd.cmd`
- Check Visual Studio version: Requires VS 2022 or later
- Verify Windows SDK: Must be 10.0 or later

### Runtime Issues
- **Trace file not found**: Verify the path in launch configuration
- **Access denied**: TTD may require administrative privileges
- **DLL not found**: Ensure TTD runtime DLLs are in the same directory as TtdDap.exe

### DAP Communication Issues
- **JSON parsing errors**: Check message format and Content-Length headers
- **Sequence errors**: Ensure proper request/response sequencing
- **Timeout issues**: Some TTD operations may take time for large traces

### VS Code Integration Issues
- **Extension not loading**: Check package.json configuration
- **Debug configuration errors**: Verify launch.json syntax
- **Adapter not found**: Ensure TtdDap.exe path is correct in the extension

## Performance Considerations

- **Large Traces**: Very large TTD traces may take time to load and navigate
- **Memory Usage**: TTD analysis can be memory-intensive
- **Step Performance**: Stepping through millions of instructions will be slow
- **Position Jumps**: Large position jumps are more efficient than many small steps

For best performance, use TTD's position-based navigation rather than single-stepping through large portions of execution.