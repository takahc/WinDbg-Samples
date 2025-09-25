# Migrating from TTDQueries to TTD-MCP Server

This guide helps users familiar with TTDQueries commands translate their workflow to use the TTD-MCP server tools.

## Command Translation Reference

### Finding Function Calls

**TTDQueries (WinDbg)**:
```
dx @$cursession.TTD.Calls("user32!MessageBoxW").OrderBy(c => c.TimeStart).Last()
```

**TTD-MCP Server**:
```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "user32!MessageBoxW",
    "order_by": "TimeStart",
    "limit": 1
  }
}
```

### Memory Access Queries

**TTDQueries (WinDbg)**:
```
dx @$cursession.TTD.Memory(&@$teb->LastErrorValue, &@$teb->LastErrorValue + 0x4, "r")
```

**TTD-MCP Server**:
```json
{
  "tool": "ttd_query_memory",
  "arguments": {
    "address_range": "0x7ffb578f1e30-0x7ffb578f1e34",
    "access_type": "r"
  }
}
```

### Error Analysis

**TTDQueries (WinDbg)**:
```
dx -g @$cursession.TTD.Calls("kernelbase!GetLastError").Where(x=> x.ReturnValue != 0).GroupBy(x => x.ReturnValue).Select(x => new { ErrorNumber = x.First().ReturnValue, ErrorCount = x.Count()}).OrderByDescending(p => p.ErrorCount)
```

**TTD-MCP Server**:
```json
{
  "tool": "ttd_analyze_errors",
  "arguments": {
    "group_by_error": true,
    "exclude_success": true
  }
}
```

### Position Navigation

**TTDQueries (WinDbg)**:
```
!tt 306600:5BF
```

**TTD-MCP Server**:
```json
{
  "tool": "ttd_navigate_position",
  "arguments": {
    "position": "306600:5BF",
    "get_context": true
  }
}
```

## Tutorial Scenario Migration

### Scenario: DShowPlayer Error Investigation

The original TTDQueries tutorial investigates the "Operation completed successfully" error in DShowPlayer. Here's how to accomplish the same investigation using TTD-MCP:

#### Step 1: Find MessageBox calls
```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "user32!MessageBoxW",
    "order_by": "TimeStart"
  }
}
```

#### Step 2: Navigate to the last call
```json
{
  "tool": "ttd_navigate_position",
  "arguments": {
    "position": "306600:5BF",
    "get_context": true,
    "context_depth": 10
  }
}
```

#### Step 3: Analyze error patterns
```json
{
  "tool": "ttd_analyze_errors",
  "arguments": {
    "group_by_error": true,
    "exclude_success": true,
    "limit": 100
  }
}
```

#### Step 4: Find the last error read before dialog
```json
{
  "tool": "ttd_query_memory",
  "arguments": {
    "address_range": "0x7ffb578f1e30-0x7ffb578f1e34",
    "access_type": "r",
    "time_range": "0:0-306600:5BF"
  }
}
```

## Advanced Query Patterns

### Complex Function Call Filtering

**TTDQueries**:
```
dx @$cursession.TTD.Calls("kernel32!CreateFileW").Where(x => x.ReturnValue == 0xFFFFFFFF)
```

**TTD-MCP**:
```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "kernel32!CreateFileW",
    "filter_expression": "x => x.ReturnValue == 0xFFFFFFFF"
  }
}
```

### Memory Range Monitoring

**TTDQueries**:
```
dx @$cursession.TTD.Memory(0x1000000, 0x1001000, "rw").Where(m => m.TimeStart < @$dialog)
```

**TTD-MCP**:
```json
{
  "tool": "ttd_query_memory",
  "arguments": {
    "address_range": "0x1000000-0x1001000",
    "access_type": "rw",
    "time_range": "0:0-306600:5BF"
  }
}
```

## Key Differences and Advantages

### TTD-MCP Server Advantages

1. **Structured Output**: Results are returned as JSON instead of WinDbg's text format
2. **Error Handling**: Better error messages and validation
3. **Aggregation**: Built-in statistics and analysis (e.g., error frequency, patterns)
4. **Consistency**: Uniform interface across all operations
5. **Integration**: Works with any MCP-compatible client (Claude, etc.)

### TTDQueries Advantages

1. **Direct Access**: Direct WinDbg integration with immediate feedback
2. **Flexibility**: Full LINQ syntax support for complex queries
3. **Real-time**: Interactive debugging with immediate command execution
4. **Advanced Features**: Access to all WinDbg extensions and commands

## Best Practices for Migration

1. **Start Simple**: Begin with basic function call queries before moving to complex patterns
2. **Use Both**: TTD-MCP for analysis, TTDQueries for interactive debugging
3. **Batch Operations**: Use TTD-MCP for repetitive analysis tasks
4. **Documentation**: TTD-MCP provides better documentation and examples
5. **Automation**: TTD-MCP enables automated analysis workflows

## Common Migration Patterns

### Pattern 1: Error Investigation Workflow
```bash
# 1. Get trace overview
ttd_trace_info → 
# 2. Analyze error patterns 
ttd_analyze_errors → 
# 3. Navigate to key positions
ttd_navigate_position → 
# 4. Query specific functions
ttd_query_calls
```

### Pattern 2: Memory Corruption Investigation
```bash
# 1. Find memory writes
ttd_query_memory → 
# 2. Navigate to suspicious writes
ttd_navigate_position → 
# 3. Check register state
ttd_inspect_registers → 
# 4. Find related function calls
ttd_query_calls
```

### Pattern 3: Performance Analysis
```bash
# 1. Query frequent functions
ttd_query_calls → 
# 2. Analyze call patterns
ttd_analyze_errors → 
# 3. Check memory hotspots
ttd_query_memory
```

## Equivalent Commands Quick Reference

| TTDQueries Command | TTD-MCP Tool | Notes |
|------------------|--------------|-------|
| `dx @$cursession.TTD.Calls()` | `ttd_query_calls` | Function call queries |
| `dx @$cursession.TTD.Memory()` | `ttd_query_memory` | Memory access queries |
| `!tt <position>` | `ttd_navigate_position` | Position navigation |
| `r` | `ttd_inspect_registers` | Register inspection |
| Custom error analysis | `ttd_analyze_errors` | Built-in error analysis |
| Various info commands | `ttd_trace_info` | Trace file information |

This migration guide helps bridge the gap between the interactive TTDQueries approach and the structured, tool-based TTD-MCP server approach.