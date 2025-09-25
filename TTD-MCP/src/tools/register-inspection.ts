/**
 * TTD Register Inspection Tool
 * Implements MCP tool for inspecting register values at specific positions in TTD traces
 */

import { Tool } from '@modelcontextprotocol/sdk/types.js';
import { WinDbgExecutor } from '../utils/windbg-executor.js';

export const registerInspectionTool: Tool = {
  name: 'ttd_inspect_registers',
  description: 'Inspect register values at current or specified position in TTD trace. Useful for examining CPU state at specific points during debugging.',
  inputSchema: {
    type: 'object',
    properties: {
      position: {
        type: 'string',
        description: 'Time travel position to inspect registers at (e.g., "306600:5BF"). If not provided, uses current position.'
      },
      registers: {
        type: 'array',
        items: { type: 'string' },
        description: 'Specific registers to inspect (e.g., ["rax", "rbx", "rcx"]). If not provided, shows all general-purpose registers.'
      },
      trace_file: {
        type: 'string',
        description: 'Path to TTD trace file (.ttd). If not provided, uses the currently loaded trace in WinDbg.'
      },
      include_flags: {
        type: 'boolean',
        description: 'Include CPU flags register information',
        default: true
      },
      include_segments: {
        type: 'boolean',
        description: 'Include segment registers (cs, ds, es, fs, gs, ss)',
        default: false
      },
      format: {
        type: 'string',
        description: 'Display format for register values',
        enum: ['hex', 'decimal', 'both'],
        default: 'hex'
      }
    }
  }
};

export async function handleRegisterInspection(args: any): Promise<any> {
  const { 
    position, 
    registers, 
    trace_file, 
    include_flags = true, 
    include_segments = false,
    format = 'hex'
  } = args;
  
  const executor = new WinDbgExecutor();
  
  // Validate trace file if provided
  if (trace_file && !(await executor.validateTraceFile(trace_file))) {
    throw new Error(`Invalid or inaccessible trace file: ${trace_file}`);
  }

  // Validate position format if provided
  if (position) {
    try {
      executor.parsePosition(position);
    } catch (error) {
      throw new Error(`Invalid position format: ${position}. Expected format: "sequence:steps" (e.g., "306600:5BF")`);
    }
  }

  try {
    const commands: string[] = [];

    // Navigate to position if specified
    if (position) {
      commands.push(`!tt ${position}`);
    }

    // Build register display commands
    if (registers && registers.length > 0) {
      // Display specific registers
      for (const reg of registers) {
        commands.push(`r ${reg}`);
      }
    } else {
      // Display all general-purpose registers
      commands.push('r');
    }

    // Add flags if requested
    if (include_flags) {
      commands.push('r efl'); // or 'r rflags' depending on architecture
    }

    // Add segment registers if requested
    if (include_segments) {
      commands.push('r cs,ds,es,fs,gs,ss');
    }

    // Add current position info
    commands.push('.time');

    const fullCommand = commands.join('; ');

    // Execute the command
    const output = trace_file 
      ? await executor.executeCommand(trace_file, fullCommand)
      : await executor.executeCommand('', fullCommand);

    // Parse the register output
    const registerInfo = parseRegisterOutput(output, format);

    return {
      position: position || 'current',
      trace_file: trace_file || 'current',
      registers: registerInfo.registers,
      flags: include_flags ? registerInfo.flags : null,
      segments: include_segments ? registerInfo.segments : null,
      current_position: registerInfo.currentPosition,
      architecture: registerInfo.architecture,
      summary: {
        position_requested: position,
        actual_position: registerInfo.currentPosition,
        register_count: Object.keys(registerInfo.registers).length,
        non_zero_registers: Object.entries(registerInfo.registers)
          .filter(([_, value]) => value !== '0' && value !== '0x0')
          .length
      }
    };

  } catch (error) {
    throw new Error(`Failed to inspect registers: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

function parseRegisterOutput(output: string, format: string): any {
  const result: any = {
    registers: {},
    flags: null,
    segments: {},
    currentPosition: null,
    architecture: 'unknown'
  };

  const lines = output.split('\n');

  for (const line of lines) {
    const trimmed = line.trim();

    // Parse time position
    const timeMatch = trimmed.match(/Time Travel Position:\s*([0-9A-Fa-f]+:[0-9A-Fa-f]+)/);
    if (timeMatch) {
      result.currentPosition = timeMatch[1];
    }

    // Parse register values
    const registerMatch = trimmed.match(/^([a-z0-9]+)\s*=\s*([0-9a-f]+)/i);
    if (registerMatch) {
      const regName = registerMatch[1].toLowerCase();
      const regValue = registerMatch[2];
      
      // Determine if this is a flag, segment, or general register
      if (regName === 'efl' || regName === 'rflags') {
        result.flags = parseFlags(regValue);
      } else if (['cs', 'ds', 'es', 'fs', 'gs', 'ss'].includes(regName)) {
        result.segments[regName] = formatRegisterValue(regValue, format);
      } else {
        result.registers[regName] = formatRegisterValue(regValue, format);
      }

      // Detect architecture based on register names
      if (regName.startsWith('r') && regName.length === 3) {
        result.architecture = 'x64';
      } else if (regName.startsWith('e') && regName.length === 3) {
        result.architecture = 'x86';
      }
    }

    // Parse multi-register line (space-separated)
    const multiRegMatch = trimmed.match(/^([a-z0-9]+)=([0-9a-f]+)\s+([a-z0-9]+)=([0-9a-f]+)/i);
    if (multiRegMatch) {
      result.registers[multiRegMatch[1].toLowerCase()] = formatRegisterValue(multiRegMatch[2], format);
      result.registers[multiRegMatch[3].toLowerCase()] = formatRegisterValue(multiRegMatch[4], format);
    }
  }

  return result;
}

function formatRegisterValue(value: string, format: string): any {
  const hexValue = `0x${value}`;
  const decValue = parseInt(value, 16);

  switch (format) {
    case 'decimal':
      return decValue;
    case 'both':
      return {
        hex: hexValue,
        decimal: decValue
      };
    case 'hex':
    default:
      return hexValue;
  }
}

function parseFlags(flagsValue: string): any {
  const flagsInt = parseInt(flagsValue, 16);
  
  // Common x86/x64 CPU flags
  const flagBits = {
    CF: (flagsInt & 0x0001) !== 0,  // Carry Flag
    PF: (flagsInt & 0x0004) !== 0,  // Parity Flag  
    AF: (flagsInt & 0x0010) !== 0,  // Auxiliary Carry Flag
    ZF: (flagsInt & 0x0040) !== 0,  // Zero Flag
    SF: (flagsInt & 0x0080) !== 0,  // Sign Flag
    TF: (flagsInt & 0x0100) !== 0,  // Trap Flag
    IF: (flagsInt & 0x0200) !== 0,  // Interrupt Flag
    DF: (flagsInt & 0x0400) !== 0,  // Direction Flag
    OF: (flagsInt & 0x0800) !== 0,  // Overflow Flag
  };

  return {
    raw_value: `0x${flagsValue}`,
    flags: flagBits,
    active_flags: Object.entries(flagBits)
      .filter(([_, isSet]) => isSet)
      .map(([flag, _]) => flag)
  };
}

/**
 * Examples of register inspection:
 * 
 * 1. Inspect all registers at current position:
 *    (no parameters)
 * 
 * 2. Inspect specific registers at a position:
 *    position: "306600:5BF"
 *    registers: ["rax", "rbx", "rcx", "rdx"]
 * 
 * 3. Full register inspection with flags and segments:
 *    position: "306600:5BF"
 *    include_flags: true
 *    include_segments: true
 *    format: "both"
 * 
 * 4. Check specific register for debugging:
 *    registers: ["rax"]
 *    format: "decimal"
 */