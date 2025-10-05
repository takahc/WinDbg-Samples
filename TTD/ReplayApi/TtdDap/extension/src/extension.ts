import * as vscode from 'vscode';
import { TtdDebugAdapterDescriptorFactory } from './ttdDebugAdapter';

export function activate(context: vscode.ExtensionContext) {
    console.log('TTD DAP Debugger extension is now active!');

    // Register the debug adapter
    const factory = new TtdDebugAdapterDescriptorFactory();
    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('ttd', factory)
    );

    // Register configuration provider
    const provider = new TtdConfigurationProvider();
    context.subscriptions.push(
        vscode.debug.registerDebugConfigurationProvider('ttd', provider)
    );
}

export function deactivate() {
    console.log('TTD DAP Debugger extension is now deactivated!');
}

class TtdConfigurationProvider implements vscode.DebugConfigurationProvider {
    /**
     * Massage a debug configuration just before a debug session is being launched,
     * e.g. add all missing attributes, substitute variables, or add default values.
     */
    resolveDebugConfiguration(
        folder: vscode.WorkspaceFolder | undefined,
        config: vscode.DebugConfiguration,
        token?: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.DebugConfiguration> {
        // If launch.json is missing or empty
        if (!config.type && !config.request && !config.name) {
            const editor = vscode.window.activeTextEditor;
            if (editor && editor.document.languageId === 'cpp') {
                config.type = 'ttd';
                config.name = 'Debug TTD Trace';
                config.request = 'launch';
                config.program = '${workspaceFolder}/trace.run';
                config.stopOnEntry = true;
            }
        }

        if (!config.program) {
            return vscode.window.showInformationMessage("Cannot find a program to debug").then((_: any) => {
                return undefined;  // abort launch
            });
        }

        return config;
    }
}