# TTD-MCP Server

A Model Context Protocol (MCP) server that provides Time Travel Debugging (TTD) query tools based on the TTDQueries functionality from the WinDbg-Samples repository.

## Overview

This MCP server exposes TTD debugging capabilities as MCP tools, allowing AI assistants and other MCP clients to:

- Query function calls in TTD traces
- Search for memory access patterns  
- Navigate to specific time positions
- Inspect register values
- Analyze error patterns
- Extract trace information

The server acts as a bridge between the powerful TTD analysis capabilities of WinDbg and modern AI-assisted debugging workflows.

## Quick Start

1. **Prerequisites**: Windows 10/11, WinDbg, Node.js 16+
2. **Install**: `npm install && npm run build`
3. **Test**: `npm test`
4. **Configure**: Add to your MCP client configuration
5. **Use**: Start querying TTD traces through your MCP client

See [INSTALL.md](INSTALL.md) for detailed setup instructions.

## Installation

```bash
npm install
npm run build
```

## Usage

Start the MCP server:

```bash
npm start
```

Or use it directly as an MCP server with compatible clients.

## Available Tools

### ttd_query_calls
Query function calls in TTD traces with optional filtering.

**Parameters:**
- `function_name` (string): Name of function to search for (e.g., "user32!MessageBoxW")
- `trace_file` (string, optional): Path to TTD trace file
- `filter_expression` (string, optional): Additional filtering expression
- `order_by` (string, optional): How to order results (e.g., "TimeStart", "TimeEnd")
- `limit` (number, optional): Maximum number of results to return

### ttd_query_memory
Query memory access patterns in TTD traces.

**Parameters:**
- `address_range` (string): Memory address range to monitor (e.g., "0x7ffb578f1e30-0x7ffb578f1e40")
- `access_type` (string): Type of access to monitor ("r", "w", "rw", "x")
- `trace_file` (string, optional): Path to TTD trace file
- `time_range` (string, optional): Time range to search within

### ttd_navigate_position
Navigate to a specific position in TTD trace.

**Parameters:**
- `position` (string): Time travel position (e.g., "306600:5BF")
- `trace_file` (string, optional): Path to TTD trace file

### ttd_inspect_registers
Inspect register values at current or specified position.

**Parameters:**
- `position` (string, optional): Time travel position
- `registers` (string[], optional): Specific registers to inspect
- `trace_file` (string, optional): Path to TTD trace file

### ttd_analyze_errors
Analyze error patterns in TTD trace, particularly GetLastError patterns.

**Parameters:**
- `trace_file` (string, optional): Path to TTD trace file
- `group_by_error` (boolean, optional): Group results by error code
- `exclude_success` (boolean, optional): Exclude successful calls (error code 0)

### ttd_trace_info
Get basic information about a TTD trace file.

**Parameters:**
- `trace_file` (string): Path to TTD trace file

## Examples

These tools are designed to be used by MCP clients. Here are some example queries you might make:

- "Find all calls to MessageBoxW in the trace"
- "Show me memory writes to address 0x7ffb578f1e30"
- "Navigate to position 306600:5BF and show the call stack"
- "Analyze all GetLastError patterns in the trace"

## Documentation

- [Installation Guide](INSTALL.md) - Detailed setup instructions for Windows
- [Basic Usage Examples](examples/basic-usage.md) - Common debugging scenarios
- [TTDQueries Migration Guide](examples/ttdqueries-migration.md) - For existing TTDQueries users
- [MCP Configuration Example](examples/mcp-config.json) - Client configuration

## Requirements

- Windows environment (TTD is Windows-specific)
- WinDbg with TTD support
- TTD trace files (.ttd)

## Architecture

This MCP server acts as a bridge between MCP clients and TTD functionality. It:

1. Receives MCP tool calls from clients
2. Translates them into appropriate TTD query operations  
3. Executes queries against TTD traces (via WinDbg command line or APIs)
4. Returns structured results back to the MCP client

## License

This project follows the same license as the parent WinDbg-Samples repository.