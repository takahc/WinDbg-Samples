# TTD-MCP Server Installation Guide

This guide provides step-by-step instructions for installing and configuring the TTD-MCP server on Windows systems.

## Prerequisites

### Required Software

1. **Windows 10/11** - TTD is Windows-specific technology
2. **WinDbg** - Download from [Microsoft Store](https://aka.ms/WinDbg) or Windows SDK
3. **Node.js** (v16 or later) - Download from [nodejs.org](https://nodejs.org/)
4. **TTD trace files** - You'll need existing .ttd files or ability to create them

### Optional but Recommended

- **Visual Studio** or **VS Code** for development
- **Windows SDK** for additional debugging tools

## Installation Steps

### 1. Clone and Setup

```bash
# Navigate to the WinDbg-Samples repository
cd /path/to/WinDbg-Samples

# Navigate to TTD-MCP directory  
cd TTD-MCP

# Install dependencies
npm install

# Build the project
npm run build
```

### 2. Verify WinDbg Installation

Ensure WinDbg is accessible from command line:

```cmd
# Test WinDbg accessibility
windbg.exe -?

# Or if installed via Windows SDK
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbg.exe" -?
```

### 3. Configure Environment Variables

Set up environment variables for easier configuration:

```cmd
# Set WinDbg path (adjust path as needed)
set WINDBG_PATH="C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbg.exe"

# Or for PowerShell
$env:WINDBG_PATH="C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbg.exe"
```

### 4. Test the Installation

```bash
# Run built-in tests
npm test

# Test the server (this will start the MCP server)
npm start
```

## Configuration for MCP Clients

### For Claude Desktop or other MCP clients

Create or update your MCP configuration file (typically `claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "ttd-mcp-server": {
      "command": "node",
      "args": ["C:\\path\\to\\WinDbg-Samples\\TTD-MCP\\dist\\index.js"],
      "env": {
        "WINDBG_PATH": "C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\windbg.exe"
      }
    }
  }
}
```

### Alternative Configuration Methods

#### 1. Global Installation (Advanced)

```bash
# Install globally (run as administrator)
npm install -g .

# Then use in MCP config
{
  "mcpServers": {
    "ttd-mcp-server": {
      "command": "ttd-mcp-server"
    }
  }
}
```

#### 2. Custom WinDbg Path

If WinDbg is installed in a non-standard location:

```json
{
  "mcpServers": {
    "ttd-mcp-server": {
      "command": "node",
      "args": ["C:\\path\\to\\TTD-MCP\\dist\\index.js"],
      "env": {
        "WINDBG_PATH": "C:\\custom\\path\\to\\windbg.exe"
      }
    }
  }
}
```

## Creating TTD Trace Files

If you don't have TTD trace files yet, here's how to create them:

### Method 1: Using WinDbg UI

1. Launch WinDbg as Administrator
2. Go to File → Start debugging → Launch executable (advanced)
3. Browse to your target executable
4. Check "Record process with Time Travel Debugging"
5. Set output directory for trace files
6. Click OK and reproduce the issue
7. Close the application - trace file (.ttd) will be generated

### Method 2: Using TTD Command Line

```cmd
# Record a process
ttd.exe -out C:\traces myapp.exe

# Record with specific options
ttd.exe -out C:\traces -children myapp.exe args
```

## Troubleshooting

### Common Issues

1. **"windbg.exe not found"**
   - Verify WinDbg installation path
   - Check WINDBG_PATH environment variable
   - Try full path in configuration

2. **"Permission denied" errors**
   - Run WinDbg/TTD as Administrator
   - Check file permissions on trace files
   - Ensure output directories are writable

3. **MCP Server not responding**
   - Check Node.js installation
   - Verify all dependencies are installed (`npm install`)
   - Check console for error messages
   - Try running server directly: `node dist/index.js`

4. **Trace file not loading**
   - Verify .ttd file is not corrupted
   - Check file size and permissions
   - Try re-indexing: open in WinDbg and run `!index -force`

### Testing Your Setup

1. **Basic server test**:
```bash
npm test
```

2. **Test with sample trace**:
   - Use the DShowPlayer sample from TTDQueries
   - Create a trace file following the tutorial
   - Try basic queries through your MCP client

3. **Manual WinDbg test**:
```cmd
windbg.exe -z "C:\traces\sample.ttd" -c "dx @$cursession.TTD; q"
```

## Performance Considerations

1. **Index your traces** - Always index TTD traces for better performance
2. **Use time ranges** - For large traces, limit queries to specific time ranges
3. **Limit result sets** - Use the `limit` parameter to avoid overwhelming results
4. **Consider file size** - Very large traces (>1GB) may need special handling

## Security Notes

1. **Administrator privileges** - TTD recording requires admin rights
2. **Sensitive data** - TTD traces contain complete execution traces
3. **File permissions** - Ensure trace files are properly secured
4. **Network access** - MCP server runs locally, no network exposure by default

## Next Steps

1. Try the [basic usage examples](examples/basic-usage.md)
2. Read the [TTDQueries tutorial](../TTDQueries/tutorial-instructions.md)
3. Experiment with your own trace files
4. Integrate with your preferred MCP client