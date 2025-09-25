# TTD Debug Adapter VS Code Extension Example

This directory contains an example VS Code extension that integrates with the TtdDap debug adapter.

## Setup

1. Build the TtdDap.exe from the parent directory
2. Install Node.js dependencies:
   ```bash
   npm install
   npm install vscode-debugadapter vscode-debugprotocol
   ```

3. Compile the TypeScript:
   ```bash
   npm run compile
   ```

4. Package the extension (optional):
   ```bash
   npm install -g vsce
   vsce package
   ```

## Usage

1. Copy the built TtdDap.exe to the extension directory
2. Install the extension in VS Code
3. Open a workspace containing TTD trace files
4. Create a launch configuration:

```json
{
    "type": "ttd",
    "request": "launch", 
    "name": "Debug TTD Trace",
    "program": "${workspaceFolder}/your-trace.ttd",
    "stopOnEntry": true
}
```

5. Start debugging with F5

## Features

- Launch TTD trace files for debugging
- Step through execution (forward only in basic implementation)
- View registers and execution position
- Basic variable inspection

This is a minimal example to demonstrate integration. A production extension would include:
- Better error handling
- Asynchronous request/response handling
- UI for TTD-specific features
- Configuration validation
- Symbol support