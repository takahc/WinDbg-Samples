#!/usr/bin/env python3
"""
Simple test script to demonstrate DAP protocol communication with TtdDap.exe
This shows how to manually send DAP messages and receive responses.
"""

import subprocess
import json
import sys
import time

def send_dap_message(process, message):
    """Send a DAP message to the process"""
    content = json.dumps(message)
    header = f"Content-Length: {len(content)}\r\n\r\n"
    full_message = header + content
    
    print(f"Sending: {message}")
    process.stdin.write(full_message.encode())
    process.stdin.flush()

def read_dap_response(process):
    """Read a DAP response from the process"""
    # Read until we find Content-Length header
    line = ""
    while "Content-Length:" not in line:
        char = process.stdout.read(1).decode()
        if char == '\n':
            if "Content-Length:" in line:
                break
            line = ""
        else:
            line += char
    
    # Extract content length
    content_length = int(line.split(":")[1].strip())
    
    # Skip the empty line after header
    process.stdout.read(2)  # \r\n
    
    # Read the JSON content
    content = process.stdout.read(content_length).decode()
    response = json.loads(content)
    
    print(f"Received: {response}")
    return response

def test_ttd_dap():
    """Test the TtdDap protocol with a simple conversation"""
    print("Starting TtdDap.exe...")
    
    # Note: Update this path to where your TtdDap.exe is built
    ttd_dap_path = "../out/x64-Release/TtdDap.exe"
    
    try:
        process = subprocess.Popen(
            [ttd_dap_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
    except FileNotFoundError:
        print(f"Error: Could not find TtdDap.exe at {ttd_dap_path}")
        print("Please build TtdDap first and update the path in this script.")
        return
    
    try:
        # 1. Initialize
        init_request = {
            "seq": 1,
            "type": "request",
            "command": "initialize",
            "arguments": {
                "clientID": "test-python",
                "adapterID": "ttd",
                "linesStartAt1": True,
                "columnsStartAt1": True
            }
        }
        send_dap_message(process, init_request)
        response = read_dap_response(process)
        
        if not response.get("success"):
            print("Initialize failed!")
            return
        
        # 2. Launch (you'll need to provide a real TTD trace file)
        trace_file = input("Enter path to TTD trace file (or press Enter to skip): ").strip()
        
        if trace_file:
            launch_request = {
                "seq": 2,
                "type": "request", 
                "command": "launch",
                "arguments": {
                    "program": trace_file,
                    "stopOnEntry": True
                }
            }
            send_dap_message(process, launch_request)
            response = read_dap_response(process)
            
            if response.get("success"):
                print("Launch successful!")
                
                # 3. Try some debugging commands
                print("\nTesting debugging commands...")
                
                # Get threads
                send_dap_message(process, {"seq": 3, "type": "request", "command": "threads"})
                read_dap_response(process)
                
                # Get stack trace
                send_dap_message(process, {
                    "seq": 4, 
                    "type": "request", 
                    "command": "stackTrace",
                    "arguments": {"threadId": 1}
                })
                read_dap_response(process)
                
                # Get scopes
                send_dap_message(process, {
                    "seq": 5,
                    "type": "request",
                    "command": "scopes", 
                    "arguments": {"frameId": 1}
                })
                read_dap_response(process)
                
                # Get variables
                send_dap_message(process, {
                    "seq": 6,
                    "type": "request",
                    "command": "variables",
                    "arguments": {"variablesReference": 1}
                })
                read_dap_response(process)
                
                # Step forward
                print("\nStepping forward...")
                send_dap_message(process, {
                    "seq": 7,
                    "type": "request",
                    "command": "next",
                    "arguments": {"threadId": 1}
                })
                read_dap_response(process)
                
                # Step backward (TTD feature!)
                print("\nStepping backward...")
                send_dap_message(process, {
                    "seq": 8,
                    "type": "request",
                    "command": "stepBack",
                    "arguments": {"threadId": 1}
                })
                read_dap_response(process)
                
            else:
                print(f"Launch failed: {response.get('message', 'Unknown error')}")
        
        # 4. Disconnect
        disconnect_request = {
            "seq": 99,
            "type": "request",
            "command": "disconnect"
        }
        send_dap_message(process, disconnect_request)
        
        print("\nTest completed!")
        
    except Exception as e:
        print(f"Error during test: {e}")
    finally:
        try:
            process.terminate()
            process.wait(timeout=5)
        except:
            process.kill()

if __name__ == "__main__":
    test_ttd_dap()