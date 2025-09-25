/**
 * Basic tests for TTD-MCP Server
 * Validates that the server can be instantiated and tools are properly configured
 */

import { functionCallsTool } from './tools/function-calls.js';
import { memoryAccessTool } from './tools/memory-access.js';
import { positionNavigationTool } from './tools/position-navigation.js';
import { registerInspectionTool } from './tools/register-inspection.js';
import { errorAnalysisTool } from './tools/error-analysis.js';
import { traceInfoTool } from './tools/trace-info.js';

/**
 * Test tool definitions and basic validation
 */
function testToolDefinitions(): boolean {
  const tools = [
    functionCallsTool,
    memoryAccessTool,
    positionNavigationTool,
    registerInspectionTool,
    errorAnalysisTool,
    traceInfoTool,
  ];

  console.log('Testing tool definitions...');
  
  for (const tool of tools) {
    // Check required properties
    if (!tool.name || !tool.description || !tool.inputSchema) {
      console.error(`Tool ${tool.name || 'unnamed'} is missing required properties`);
      return false;
    }

    // Check that inputSchema is valid
    if (tool.inputSchema.type !== 'object') {
      console.error(`Tool ${tool.name} has invalid inputSchema type`);
      return false;
    }

    console.log(`✓ Tool ${tool.name} is properly defined`);
  }

  return true;
}

/**
 * Test that all tools have unique names
 */
function testToolNames(): boolean {
  const tools = [
    functionCallsTool,
    memoryAccessTool,
    positionNavigationTool,
    registerInspectionTool,
    errorAnalysisTool,
    traceInfoTool,
  ];

  console.log('Testing tool name uniqueness...');
  
  const names = tools.map(tool => tool.name);
  const uniqueNames = new Set(names);
  
  if (names.length !== uniqueNames.size) {
    console.error('Duplicate tool names found');
    return false;
  }

  console.log('✓ All tool names are unique');
  return true;
}

/**
 * Test basic tool schema validation
 */
function testToolSchemas(): boolean {
  const tools = [
    functionCallsTool,
    memoryAccessTool,
    positionNavigationTool,
    registerInspectionTool,
    errorAnalysisTool,
    traceInfoTool,
  ];

  console.log('Testing tool schemas...');
  
  for (const tool of tools) {
    const schema = tool.inputSchema;
    
    // Check that required properties exist
    if (schema.required && Array.isArray(schema.required)) {
      for (const requiredProp of schema.required) {
        if (!schema.properties || !schema.properties[requiredProp]) {
          console.error(`Tool ${tool.name} requires property ${requiredProp} but it's not defined`);
          return false;
        }
      }
    }

    console.log(`✓ Tool ${tool.name} schema is valid`);
  }

  return true;
}

/**
 * Test that TTD-specific functionality is properly abstracted
 */
async function testTTDIntegration(): Promise<boolean> {
  console.log('Testing TTD integration abstractions...');
  
  // Test that we can import and use the WinDbgExecutor
  try {
    const { WinDbgExecutor } = await import('./utils/windbg-executor.js');
    const executor = new WinDbgExecutor();
    
    // Test position parsing
    const position = executor.parsePosition('306600:5BF');
    if (position.sequence !== '306600' || position.steps !== '5BF') {
      console.error('Position parsing failed');
      return false;
    }
    
    console.log('✓ TTD position parsing works correctly');
    
    // Test query building
    const query = executor.buildFunctionCallQuery('user32!MessageBoxW', { limit: 10 });
    if (!query.includes('MessageBoxW') || !query.includes('Take(10)')) {
      console.error('Query building failed');
      return false;
    }
    
    console.log('✓ TTD query building works correctly');
    
  } catch (error) {
    console.error('TTD integration test failed:', error);
    return false;
  }

  return true;
}

/**
 * Run all tests
 */
async function runTests(): Promise<void> {
  console.log('Running TTD-MCP Server Tests...\n');

  let allPassed = true;

  allPassed = testToolDefinitions() && allPassed;
  console.log();
  
  allPassed = testToolNames() && allPassed;
  console.log();
  
  allPassed = testToolSchemas() && allPassed;
  console.log();
  
  allPassed = await testTTDIntegration() && allPassed;
  console.log();

  if (allPassed) {
    console.log('🎉 All tests passed! TTD-MCP Server is ready for use.');
    process.exit(0);
  } else {
    console.log('❌ Some tests failed. Please review the errors above.');
    process.exit(1);
  }
}

// Run tests if this file is executed directly
if (import.meta.url === `file://${process.argv[1]}`) {
  runTests().catch((error) => {
    console.error('Test execution failed:', error);
    process.exit(1);
  });
}