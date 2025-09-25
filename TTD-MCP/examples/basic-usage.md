# TTD-MCP Server Basic Usage Examples

This document provides examples of how to use the TTD-MCP server tools for various debugging scenarios.

## Prerequisites

1. Windows environment with WinDbg installed
2. TTD trace files (.ttd) available for analysis
3. TTD-MCP server running and connected to an MCP client

## Common Usage Patterns

### 1. Investigating Error Messages

**Scenario**: You have an application showing "Operation completed successfully" but the operation actually failed.

```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "user32!MessageBoxW",
    "order_by": "TimeStart",
    "limit": 10
  }
}
```

Then navigate to the last message box call:

```json
{
  "tool": "ttd_navigate_position", 
  "arguments": {
    "position": "306600:5BF",
    "get_context": true
  }
}
```

### 2. Memory Corruption Debugging

**Scenario**: Debugging memory corruption at a specific address.

```json
{
  "tool": "ttd_query_memory",
  "arguments": {
    "address_range": "0x7ffb578f1e30",
    "access_type": "w",
    "limit": 20
  }
}
```

Follow up with detailed analysis of the write patterns:

```json
{
  "tool": "ttd_query_memory",
  "arguments": {
    "address_range": "0x7ffb578f1e30-0x7ffb578f1e40", 
    "access_type": "rw",
    "time_range": "306000:0-307000:0"
  }
}
```

### 3. API Failure Analysis

**Scenario**: Understanding why CreateFile calls are failing.

```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "kernel32!CreateFileW",
    "filter_expression": "x => x.ReturnValue == 0xFFFFFFFF",
    "order_by": "TimeStart"
  }
}
```

Then analyze the error patterns:

```json
{
  "tool": "ttd_analyze_errors",
  "arguments": {
    "group_by_error": true,
    "exclude_success": true
  }
}
```

### 4. Thread Synchronization Issues

**Scenario**: Investigating race conditions between threads.

```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "kernel32!WaitForSingleObject",
    "order_by": "TimeStart"
  }
}
```

Cross-reference with memory access to shared data:

```json
{
  "tool": "ttd_query_memory",
  "arguments": {
    "address_range": "0x12345678-0x12345680",
    "access_type": "rw"
  }
}
```

### 5. Performance Investigation

**Scenario**: Finding hot spots or frequently called functions.

```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "ntdll!RtlAllocateHeap",
    "limit": 1000
  }
}
```

### 6. Register State Analysis

**Scenario**: Examining CPU state at a crash point.

```json
{
  "tool": "ttd_navigate_position",
  "arguments": {
    "position": "47e8:6c4"
  }
}
```

```json
{
  "tool": "ttd_inspect_registers",
  "arguments": {
    "include_flags": true,
    "format": "both"
  }
}
```

## Advanced Workflows

### Complete Error Investigation Workflow

1. **Get trace overview**:
```json
{
  "tool": "ttd_trace_info",
  "arguments": {
    "trace_file": "C:\\traces\\myapp.ttd",
    "include_threads": true
  }
}
```

2. **Find error patterns**:
```json
{
  "tool": "ttd_analyze_errors",
  "arguments": {
    "group_by_error": true,
    "limit": 100
  }
}
```

3. **Navigate to most frequent error**:
```json
{
  "tool": "ttd_navigate_position",
  "arguments": {
    "position": "306600:5BF",
    "get_context": true,
    "context_depth": 15
  }
}
```

4. **Examine the call that set the error**:
```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "kernel32!SetLastError",
    "order_by": "TimeStart"
  }
}
```

### Memory Layout Investigation

1. **Find all memory allocations**:
```json
{
  "tool": "ttd_query_calls",
  "arguments": {
    "function_name": "ntdll!RtlAllocateHeap"
  }
}
```

2. **Track specific buffer usage**:
```json
{
  "tool": "ttd_query_memory",
  "arguments": {
    "address_range": "0x1000000-0x1001000",
    "access_type": "rw"
  }
}
```

3. **Examine register state during allocation**:
```json
{
  "tool": "ttd_inspect_registers",
  "arguments": {
    "position": "305000:1A0",
    "registers": ["rcx", "rdx", "r8", "r9"]
  }
}
```

## Tips for Effective Usage

1. **Start with trace info** to understand the scope and characteristics of your trace
2. **Use time ranges** for large traces to improve performance  
3. **Combine tools** - use function call queries to find interesting positions, then navigate and inspect
4. **Filter aggressively** - use filter expressions to narrow down to relevant results
5. **Group and aggregate** - use error analysis and statistics to understand patterns