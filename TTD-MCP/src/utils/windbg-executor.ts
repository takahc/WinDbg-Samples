/**
 * Utility for executing WinDbg commands and parsing TTD query results
 */

import { spawn, SpawnOptions } from 'child_process';
import { promises as fs } from 'fs';
import { join, dirname } from 'path';
import { TTDPosition, TTDFunctionCall, TTDMemoryAccess, TTDTraceInfo, TTDQueryResult } from '../types/ttd.js';

export class WinDbgExecutor {
  private windbgPath: string;
  private defaultTimeout: number = 30000; // 30 seconds

  constructor(windbgPath?: string) {
    // Default to common WinDbg installation paths
    this.windbgPath = windbgPath || 'windbg.exe';
  }

  /**
   * Execute a WinDbg command against a TTD trace file
   */
  async executeCommand(traceFile: string, command: string, timeout?: number): Promise<string> {
    return new Promise((resolve, reject) => {
      const args = [
        '-z', traceFile,  // Open trace file
        '-c', `${command}; q`, // Execute command and quit
        '-logo', // Log output
      ];

      const options: SpawnOptions = {
        stdio: ['pipe', 'pipe', 'pipe'],
        timeout: timeout || this.defaultTimeout,
      };

      const windbg = spawn(this.windbgPath, args, options);
      let output = '';
      let errorOutput = '';

      windbg.stdout?.on('data', (data) => {
        output += data.toString();
      });

      windbg.stderr?.on('data', (data) => {
        errorOutput += data.toString();
      });

      windbg.on('close', (code) => {
        if (code === 0) {
          resolve(output);
        } else {
          reject(new Error(`WinDbg exited with code ${code}. Error: ${errorOutput}`));
        }
      });

      windbg.on('error', (error) => {
        reject(new Error(`Failed to spawn WinDbg: ${error.message}`));
      });
    });
  }

  /**
   * Parse TTD position string into structured format
   */
  parsePosition(positionStr: string): TTDPosition {
    const match = positionStr.match(/([0-9A-Fa-f]+):([0-9A-Fa-f]+)/);
    if (!match) {
      throw new Error(`Invalid position format: ${positionStr}`);
    }

    return {
      sequence: match[1],
      steps: match[2],
      formatted: positionStr.toUpperCase(),
    };
  }

  /**
   * Parse function call query results from WinDbg output
   */
  parseFunctionCalls(output: string): TTDFunctionCall[] {
    const calls: TTDFunctionCall[] = [];
    
    // This is a simplified parser - in practice, you'd need more robust parsing
    // of WinDbg's dx command output format
    const lines = output.split('\n');
    let currentCall: Partial<TTDFunctionCall> = {};

    for (const line of lines) {
      const trimmed = line.trim();
      
      if (trimmed.includes('EventType') && trimmed.includes('Call')) {
        if (Object.keys(currentCall).length > 0) {
          calls.push(currentCall as TTDFunctionCall);
        }
        currentCall = { eventType: 'Call' };
      } else if (trimmed.startsWith('ThreadId')) {
        const match = trimmed.match(/ThreadId\s*:\s*(.+)/);
        if (match) currentCall.threadId = match[1].trim();
      } else if (trimmed.startsWith('TimeStart')) {
        const match = trimmed.match(/TimeStart\s*:\s*([0-9A-Fa-f]+:[0-9A-Fa-f]+)/);
        if (match) currentCall.timeStart = this.parsePosition(match[1]);
      } else if (trimmed.startsWith('TimeEnd')) {
        const match = trimmed.match(/TimeEnd\s*:\s*([0-9A-Fa-f]+:[0-9A-Fa-f]+)/);
        if (match) currentCall.timeEnd = this.parsePosition(match[1]);
      } else if (trimmed.startsWith('Function')) {
        const match = trimmed.match(/Function\s*:\s*(.+)/);
        if (match) currentCall.function = match[1].trim();
      } else if (trimmed.startsWith('ReturnValue')) {
        const match = trimmed.match(/ReturnValue\s*:\s*(.+)/);
        if (match) currentCall.returnValue = match[1].trim();
      }
    }

    if (Object.keys(currentCall).length > 0) {
      calls.push(currentCall as TTDFunctionCall);
    }

    return calls;
  }

  /**
   * Parse memory access query results from WinDbg output  
   */
  parseMemoryAccesses(output: string): TTDMemoryAccess[] {
    const accesses: TTDMemoryAccess[] = [];
    
    // Similar parsing logic for memory access results
    const lines = output.split('\n');
    let currentAccess: Partial<TTDMemoryAccess> = {};

    for (const line of lines) {
      const trimmed = line.trim();
      
      if (trimmed.includes('EventType') && trimmed.includes('Memory')) {
        if (Object.keys(currentAccess).length > 0) {
          accesses.push(currentAccess as TTDMemoryAccess);
        }
        currentAccess = { eventType: 'Memory' };
      }
      // Add more parsing logic for memory access fields...
    }

    return accesses;
  }

  /**
   * Validate that a trace file exists and is accessible
   */
  async validateTraceFile(traceFile: string): Promise<boolean> {
    try {
      const stats = await fs.stat(traceFile);
      return stats.isFile() && traceFile.toLowerCase().endsWith('.ttd');
    } catch {
      return false;
    }
  }

  /**
   * Build a dx query command for function calls
   */
  buildFunctionCallQuery(functionName: string, options: {
    filterExpression?: string;
    orderBy?: string;
    limit?: number;
  } = {}): string {
    let query = `@$cursession.TTD.Calls("${functionName}")`;
    
    if (options.filterExpression) {
      query += `.Where(${options.filterExpression})`;
    }
    
    if (options.orderBy) {
      query += `.OrderBy(c => c.${options.orderBy})`;
    }
    
    if (options.limit) {
      query += `.Take(${options.limit})`;
    }

    return `dx ${query}`;
  }

  /**
   * Build a dx query command for memory access
   */
  buildMemoryQuery(addressRange: string, accessType: string, options: {
    timeRange?: string;
  } = {}): string {
    const [startAddr, endAddr] = addressRange.split('-');
    let query = `@$cursession.TTD.Memory(${startAddr}, ${endAddr || startAddr + ' + 0x8'}, "${accessType}")`;
    
    if (options.timeRange) {
      // Add time range filtering if needed
      const [startTime, endTime] = options.timeRange.split('-');
      query += `.Where(m => m.TimeStart >= ${startTime} && m.TimeEnd <= ${endTime})`;
    }

    return `dx ${query}`;
  }
}