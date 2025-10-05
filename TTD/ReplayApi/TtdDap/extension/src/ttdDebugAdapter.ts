import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';

export class TtdDebugAdapterDescriptorFactory implements vscode.DebugAdapterDescriptorFactory {
    createDebugAdapterDescriptor(
        session: vscode.DebugSession,
        executable: vscode.DebugAdapterExecutable | undefined
    ): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {
        // Find the TtdDap.exe executable
        const ttdDapPath = this.findTtdDapExecutable();
        
        if (!ttdDapPath) {
            vscode.window.showErrorMessage('TtdDap.exe not found. Please build the TTD DAP server first.');
            return null;
        }

        // Return executable descriptor that launches TtdDap.exe
        return new vscode.DebugAdapterExecutable(ttdDapPath, []);
    }

    private findTtdDapExecutable(): string | null {
        // Look for TtdDap.exe in common build output directories
        const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
        if (!workspaceFolder) {
            return null;
        }

        const possiblePaths = [
            path.join("D:/prog/WinDbg-Samples/TTD/ReplayApi/TtdDap/out/x64-Debug/TtdDap.exe"),
            path.join(workspaceFolder.uri.fsPath, 'TTD', 'ReplayApi', 'TtdDap', 'x64', 'Debug', 'TtdDap.exe'),
            path.join(workspaceFolder.uri.fsPath, 'TTD', 'ReplayApi', 'TtdDap', 'x64', 'Release', 'TtdDap.exe'),
            path.join(workspaceFolder.uri.fsPath, 'TTD', 'ReplayApi', 'TtdDap', 'Debug', 'TtdDap.exe'),
            path.join(workspaceFolder.uri.fsPath, 'TTD', 'ReplayApi', 'TtdDap', 'Release', 'TtdDap.exe'),
            path.join(workspaceFolder.uri.fsPath, 'TTD', 'out', 'x64-Debug', 'TtdDap.exe'),
            path.join(workspaceFolder.uri.fsPath, 'TTD', 'out', 'x64-Release', 'TtdDap.exe')
        ];

        for (const execPath of possiblePaths) {
            if (fs.existsSync(execPath)) {
                return execPath;
            }
        }

        return null;
    }
}