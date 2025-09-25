/**
 * TTD Position Navigation Tool
 * Implements MCP tool for navigating to specific positions in TTD traces
 */

import { Tool } from '@modelcontextprotocol/sdk/types.js';
import { WinDbgExecutor } from '../utils/windbg-executor.js';

export const positionNavigationTool: Tool = {
  name: 'ttd_navigate_position',
  description: 'Navigate to a specific position in TTD trace and get context information. Useful for jumping to specific points in time during debugging.',
  inputSchema: {
    type: 'object',
    properties: {
      position: {
        type: 'string',
        description: 'Time travel position in format "sequence:steps" (e.g., "306600:5BF")'
      },
      trace_file: {
        type: 'string',
        description: 'Path to TTD trace file (.ttd). If not provided, uses the currently loaded trace in WinDbg.'
      },
      get_context: {
        type: 'boolean',
        description: 'Whether to retrieve additional context information (call stack, registers, nearby code)',
        default: true
      },
      context_depth: {
        type: 'number',
        description: 'Depth of call stack to retrieve (default: 10)',
        minimum: 1,
        maximum: 50,
        default: 10
      }
    },
    required: ['position']
  }
};

export async function handlePositionNavigation(args: any): Promise<any> {
  const { position, trace_file, get_context = true, context_depth = 10 } = args;
  
  const executor = new WinDbgExecutor();
  
  // Validate trace file if provided
  if (trace_file && !(await executor.validateTraceFile(trace_file))) {
    throw new Error(`Invalid or inaccessible trace file: ${trace_file}`);
  }

  // Validate position format
  try {
    executor.parsePosition(position);
  } catch (error) {
    throw new Error(`Invalid position format: ${position}. Expected format: "sequence:steps" (e.g., "306600:5BF")`);
  }

  try {
    // Navigate to the position
    const navigationCommand = `!tt ${position}`;
    
    const commands = [navigationCommand];
    
    if (get_context) {
      // Add commands to get context information
      commands.push(
        'r',  // Show registers
        `k ${context_depth}`,  // Show call stack
        'u @rip L5',  // Disassemble 5 instructions at current position
        '.time',  // Show current time position
        '!thread'  // Show thread information
      );
    }

    const fullCommand = commands.join('; ');
    
    // Execute the command
    const output = trace_file 
      ? await executor.executeCommand(trace_file, fullCommand)
      : await executor.executeCommand('', fullCommand);

    // Parse the output
    const navigationResult = parseNavigationOutput(output, position, get_context);

    return {
      navigation_successful: true,
      target_position: position,
      current_position: navigationResult.currentPosition,
      trace_file: trace_file || 'current',
      context: get_context ? navigationResult.context : null,
      summary: {
        position,
        thread_id: navigationResult.context?.threadId,
        instruction_pointer: navigationResult.context?.registers?.rip || navigationResult.context?.registers?.eip,
        current_function: navigationResult.context?.callStack?.[0]?.function
      }
    };
  } catch (error) {
    throw new Error(`Failed to navigate to position: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

function parseNavigationOutput(output: string, targetPosition: string, getContext: boolean): any {
  const result: any = {
    currentPosition: targetPosition,
    context: null
  };

  if (!getContext) {
    return result;
  }

  const lines = output.split('\n');
  const context: any = {
    registers: {},
    callStack: [],
    disassembly: [],
    threadId: null,
    timeInfo: null
  };

  let section = 'none';
  
  for (const line of lines) {
    const trimmed = line.trim();
    
    // Detect sections
    if (trimmed.match(/^[a-z0-9]+\s*=/)) {
      section = 'registers';
    } else if (trimmed.match(/^\d+\s+[0-9a-f]+/)) {
      section = 'callstack';
    } else if (trimmed.match(/^[0-9a-f]+`[0-9a-f]+/)) {
      section = 'disassembly';
    } else if (trimmed.includes('Thread ID:')) {
      section = 'thread';
    } else if (trimmed.includes('Time Travel Position:')) {
      section = 'time';
    }

    // Parse based on current section
    switch (section) {
      case 'registers':
        const regMatch = trimmed.match(/^([a-z0-9]+)\s*=\s*([0-9a-f]+)/i);
        if (regMatch) {
          context.registers[regMatch[1]] = regMatch[2];
        }
        break;
        
      case 'callstack':
        const stackMatch = trimmed.match(/^(\d+)\s+([0-9a-f]+`[0-9a-f]+)\s+(.+)/);
        if (stackMatch) {
          context.callStack.push({
            frame: parseInt(stackMatch[1]),
            address: stackMatch[2],
            function: stackMatch[3]
          });
        }
        break;
        
      case 'disassembly':
        const asmMatch = trimmed.match(/^([0-9a-f]+)`([0-9a-f]+)\s+([0-9a-f]+)\s+(.+)/);
        if (asmMatch) {
          context.disassembly.push({
            address: `${asmMatch[1]}\`${asmMatch[2]}`,
            bytes: asmMatch[3],
            instruction: asmMatch[4]
          });
        }
        break;
        
      case 'thread':
        const threadMatch = trimmed.match(/Thread ID:\s*([0-9a-f]+)/i);
        if (threadMatch) {
          context.threadId = threadMatch[1];
        }
        break;
        
      case 'time':
        const timeMatch = trimmed.match(/Time Travel Position:\s*([0-9A-Fa-f]+:[0-9A-Fa-f]+)/);
        if (timeMatch) {
          context.timeInfo = timeMatch[1];
          result.currentPosition = timeMatch[1];
        }
        break;
    }
  }

  result.context = context;
  return result;
}

/**
 * Examples of common position navigation:
 * 
 * 1. Navigate to specific position:
 *    position: "306600:5BF"
 * 
 * 2. Navigate and get minimal context:
 *    position: "306600:5BF"
 *    get_context: true
 *    context_depth: 5
 * 
 * 3. Navigate to crash point:
 *    position: "47e8:6c4"
 *    get_context: true
 * 
 * 4. Navigate to specific call from query result:
 *    position: "30CADE:80"  // TimeEnd from function call query
 */