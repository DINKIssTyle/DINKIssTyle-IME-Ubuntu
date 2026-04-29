// Created by DINKIssTyle on 2026. Copyright (C) 2026 DINKI'ssTyle. All rights reserved.

/**
 * DKST IME Indicator - GNOME Shell Extension
 *
 * Replaces the default text-based input mode indicator in the GNOME Shell
 * top panel with SVG icons (KO/EN) for the DKST Korean IME.
 *
 * This extension monitors IBus property updates from the DKST engine
 * and swaps the panel icon accordingly.
 *
 * Features:
 *   - KO/EN icon indicator in the top panel
 *   - Popup menu with Settings, Dictionary Editor, IBus icon toggle
 *   - Persistent IBus icon hide preference
 */

import GObject from 'gi://GObject';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import St from 'gi://St';
import Clutter from 'gi://Clutter';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';

const DKST_ENGINE_NAME = 'dinkisstyle';
const ICON_DIR = '/usr/share/ibus-dkst';
const SETUP_PATH = '/usr/share/ibus-dkst/setup.py';
const HANJA_EDITOR_PATH = '/usr/share/ibus-dkst/hanja_editor.py';
const CONFIG_DIR = GLib.get_home_dir() + '/.config/ibus-dkst';
const EXT_CONFIG_FILE = CONFIG_DIR + '/extension.json';

/**
 * Load extension configuration from file.
 * @returns {Object} Configuration object with default values
 */
function _loadExtConfig() {
    const defaults = {hideIBusIcon: false};
    try {
        const file = Gio.File.new_for_path(EXT_CONFIG_FILE);
        if (file.query_exists(null)) {
            const [ok, contents] = file.load_contents(null);
            if (ok) {
                const json = JSON.parse(new TextDecoder().decode(contents));
                return {...defaults, ...json};
            }
        }
    } catch (e) {
        console.error(`[DKST] Failed to load extension config: ${e.message}`);
    }
    return defaults;
}

/**
 * Save extension configuration to file.
 * @param {Object} config - Configuration object to save
 */
function _saveExtConfig(config) {
    try {
        // Ensure config directory exists
        const dir = Gio.File.new_for_path(CONFIG_DIR);
        if (!dir.query_exists(null)) {
            dir.make_directory_with_parents(null);
        }

        const file = Gio.File.new_for_path(EXT_CONFIG_FILE);
        const data = JSON.stringify(config, null, 2);
        file.replace_contents(
            new TextEncoder().encode(data),
            null, false,
            Gio.FileCreateFlags.REPLACE_DESTINATION,
            null
        );
    } catch (e) {
        console.error(`[DKST] Failed to save extension config: ${e.message}`);
    }
}

/**
 * Panel indicator button that shows KO/EN icon with popup menu.
 */
const DkstIndicator = GObject.registerClass(
class DkstIndicator extends PanelMenu.Button {
    _init(extensionPath) {
        super._init(0.5, 'DKST IME Indicator');

        this._extensionPath = extensionPath;

        // Determine icon paths: prefer installed location, fallback to extension dir
        // Use symbolic icons for proper GNOME Shell theme recoloring
        this._koIconPath = `${ICON_DIR}/KO-symbolic.svg`;
        this._enIconPath = `${ICON_DIR}/EN-symbolic.svg`;

        // Fallback to extension-bundled icons if system icons don't exist
        const koFile = Gio.File.new_for_path(this._koIconPath);
        if (!koFile.query_exists(null)) {
            this._koIconPath = `${extensionPath}/icons/KO-symbolic.svg`;
            this._enIconPath = `${extensionPath}/icons/EN-symbolic.svg`;
        }

        // Create icon widget
        // Use 'system-status-icon' style class for GNOME Shell theme recoloring
        this._icon = new St.Icon({
            gicon: Gio.icon_new_for_string(this._koIconPath),
            style_class: 'system-status-icon dkst-indicator-icon',
            icon_size: 16,
        });
        this.add_child(this._icon);
        this.add_style_class_name('dkst-indicator-button');

        this._isHangulMode = true;

        // Load saved preferences
        this._config = _loadExtConfig();

        // Build popup menu
        this._buildMenu();
    }

    /**
     * Build the popup menu with all items.
     */
    _buildMenu() {
        // ── Mode display (informational header) ──
        this._modeItem = new PopupMenu.PopupMenuItem('한글 모드', {reactive: false});
        this._modeItem.label.add_style_class_name('dkst-menu-header');
        this.menu.addMenuItem(this._modeItem);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        // ── Settings (환경설정) ──
        const settingsItem = new PopupMenu.PopupMenuItem('환경설정');
        settingsItem.connect('activate', () => this._launchSetup());
        this.menu.addMenuItem(settingsItem);

        // ── Dictionary Editor (사전편집기) ──
        const dictItem = new PopupMenu.PopupMenuItem('사전 편집기');
        dictItem.connect('activate', () => this._launchDictEditor());
        this.menu.addMenuItem(dictItem);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        // ── IBus Icon Hide Toggle ──
        this._hideIBusSwitch = new PopupMenu.PopupSwitchMenuItem(
            'IBus 아이콘 숨기기',
            this._config.hideIBusIcon
        );
        this._hideIBusSwitch.connect('toggled', (_item, state) => {
            this._config.hideIBusIcon = state;
            _saveExtConfig(this._config);
            this._applyIBusIconVisibility();
        });
        this.menu.addMenuItem(this._hideIBusSwitch);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        // ── About (정보) ──
        const aboutItem = new PopupMenu.PopupMenuItem('DKST 정보');
        aboutItem.connect('activate', () => this._showAbout());
        this.menu.addMenuItem(aboutItem);
    }

    /**
     * Update the displayed mode in menu header and icon.
     */
    setHangulMode(isHangul) {
        if (this._isHangulMode === isHangul)
            return;

        this._isHangulMode = isHangul;
        const iconPath = isHangul ? this._koIconPath : this._enIconPath;
        this._icon.gicon = Gio.icon_new_for_string(iconPath);

        // Update menu header text
        if (this._modeItem) {
            this._modeItem.label.set_text(isHangul ? '한글 모드' : '영문 모드');
        }
    }

    getHangulMode() {
        return this._isHangulMode;
    }

    /**
     * Launch the DKST setup/preferences window.
     */
    _launchSetup() {
        try {
            // Try system-installed path first
            const sysFile = Gio.File.new_for_path(SETUP_PATH);
            if (sysFile.query_exists(null)) {
                GLib.spawn_command_line_async(`python3 ${SETUP_PATH}`);
            } else {
                // Fallback to source directory relative path
                const localPath = `${this._extensionPath}/../setup.py`;
                GLib.spawn_command_line_async(`python3 ${localPath}`);
            }
        } catch (e) {
            console.error(`[DKST] Failed to launch setup: ${e.message}`);
        }
    }

    /**
     * Launch the DKST hanja dictionary editor.
     */
    _launchDictEditor() {
        try {
            // Try system-installed path first
            const sysFile = Gio.File.new_for_path(HANJA_EDITOR_PATH);
            if (sysFile.query_exists(null)) {
                GLib.spawn_command_line_async(`python3 ${HANJA_EDITOR_PATH}`);
            } else {
                // Fallback to source directory relative path
                const localPath = `${this._extensionPath}/../hanja_editor.py`;
                GLib.spawn_command_line_async(`python3 ${localPath}`);
            }
        } catch (e) {
            console.error(`[DKST] Failed to launch dictionary editor: ${e.message}`);
        }
    }

    /**
     * Apply IBus icon visibility based on current config.
     * Hides/shows the GNOME Shell IBus input source indicator.
     *
     * This follows Customize IBus: toggle the keyboard indicator container
     * inside GNOME Shell instead of changing IBus' persistent gsettings.
     */
    _applyIBusIconVisibility() {
        try {
            const inputSourceIndicator = Main.panel.statusArea['keyboard'];
            if (inputSourceIndicator?.container)
                inputSourceIndicator.container.visible = !this._config.hideIBusIcon;
        } catch (e) {
            console.error(`[DKST] Failed to toggle IBus icon: ${e.message}`);
        }
    }

    /**
     * Show a simple About notification.
     */
    _showAbout() {
        Main.notify(
            'DKST 한국어 입력기',
            'DKST Korean Input Method\nVersion 1.0\n© 2026 DINKI\'ssTyle'
        );
    }

    /**
     * Get the current IBus icon hide state.
     */
    getHideIBusIcon() {
        return this._config.hideIBusIcon;
    }
});


export default class DkstIndicatorExtension extends Extension {
    constructor(metadata) {
        super(metadata);
        this._indicator = null;
        this._ibusManager = null;
        this._propertyRegisteredId = 0;
        this._propertyUpdatedId = 0;
        this._sourceChangedId = 0;
        this._isDkstActive = false;
        this._originalIndicator = null;
    }

    enable() {
        // Create our custom indicator
        this._indicator = new DkstIndicator(this.path);

        // Initially hidden until DKST engine is confirmed active
        this._indicator.hide();

        // Add to panel (right box, index 0 = leftmost in right box)
        Main.panel.addToStatusArea('dkst-indicator', this._indicator, 0, 'right');

        // Get the IBusManager from GNOME Shell internals
        this._connectIBusSignals();

        // Monitor current input source changes
        this._connectInputSourceSignals();

        // Check if DKST is already active
        this._checkCurrentSource();

        // Apply saved IBus icon visibility preference
        this._indicator._applyIBusIconVisibility();
    }

    disable() {
        this._disconnectIBusSignals();
        this._disconnectInputSourceSignals();

        // Restore default indicator visibility if we hid it
        this._showDefaultIndicator();

        // Restore IBus icon visibility on disable
        this._showIBusIndicatorContainer();

        if (this._indicator) {
            this._indicator.destroy();
            this._indicator = null;
        }

        this._isDkstActive = false;
    }

    /**
     * Connect to GNOME Shell's internal IBusManager signals.
     * These fire when IBus engines register/update properties.
     */
    _connectIBusSignals() {
        // Access GNOME Shell's internal IBusManager
        // This is the same manager that keyboard.js uses
        const ibusManager = this._getIBusManager();
        if (!ibusManager)
            return;

        this._ibusManager = ibusManager;

        this._propertyRegisteredId = ibusManager.connect(
            'properties-registered',
            (_im, engineName, props) => {
                this._onPropertiesRegistered(engineName, props);
            }
        );

        this._propertyUpdatedId = ibusManager.connect(
            'property-updated',
            (_im, engineName, prop) => {
                this._onPropertyUpdated(engineName, prop);
            }
        );
    }

    _disconnectIBusSignals() {
        if (this._ibusManager) {
            if (this._propertyRegisteredId > 0) {
                this._ibusManager.disconnect(this._propertyRegisteredId);
                this._propertyRegisteredId = 0;
            }
            if (this._propertyUpdatedId > 0) {
                this._ibusManager.disconnect(this._propertyUpdatedId);
                this._propertyUpdatedId = 0;
            }
            this._ibusManager = null;
        }
    }

    /**
     * Connect to InputSourceManager to detect when DKST engine
     * becomes active or inactive.
     */
    _connectInputSourceSignals() {
        const inputSourceManager = this._getInputSourceManager();
        if (!inputSourceManager)
            return;

        this._sourceChangedId = inputSourceManager.connect(
            'current-source-changed',
            () => this._checkCurrentSource()
        );
    }

    _disconnectInputSourceSignals() {
        if (this._sourceChangedId > 0) {
            const inputSourceManager = this._getInputSourceManager();
            if (inputSourceManager) {
                inputSourceManager.disconnect(this._sourceChangedId);
            }
            this._sourceChangedId = 0;
        }
    }

    /**
     * Check if the currently active input source is the DKST engine.
     */
    _checkCurrentSource() {
        const inputSourceManager = this._getInputSourceManager();
        if (!inputSourceManager)
            return;

        const currentSource = inputSourceManager.currentSource;
        const isDkst = currentSource &&
                       currentSource.type === 'ibus' &&
                       currentSource.id === DKST_ENGINE_NAME;

        if (isDkst && !this._isDkstActive) {
            this._isDkstActive = true;
            this._indicator?.show();
            this._hideDefaultIndicator();
        } else if (!isDkst && this._isDkstActive) {
            this._isDkstActive = false;
            this._indicator?.hide();
            this._showDefaultIndicator();
        }
    }

    /**
     * Called when an IBus engine registers its properties.
     * We scan for the InputMode property to get initial state.
     */
    _onPropertiesRegistered(engineName, props) {
        if (engineName !== DKST_ENGINE_NAME)
            return;

        if (!props)
            return;

        // Scan properties for InputMode
        let p;
        for (let i = 0; (p = props.get(i)) != null; ++i) {
            if (p.get_key() === 'InputMode') {
                this._updateFromProperty(p);
                break;
            }
        }

        this._isDkstActive = true;
        this._indicator?.show();
        this._hideDefaultIndicator();
    }

    /**
     * Called when an IBus engine updates a property.
     * We look for InputMode changes from the DKST engine.
     */
    _onPropertyUpdated(engineName, prop) {
        if (engineName !== DKST_ENGINE_NAME)
            return;

        if (prop.get_key() !== 'InputMode')
            return;

        this._updateFromProperty(prop);
    }

    /**
     * Update the icon based on the InputMode property.
     * We check the symbol text to determine the current mode.
     */
    _updateFromProperty(prop) {
        if (!this._indicator)
            return;

        let symbolText = '';
        if (prop.get_symbol)
            symbolText = prop.get_symbol().get_text();
        else if (prop.get_label)
            symbolText = prop.get_label().get_text();

        // Determine mode from symbol text
        // The engine sets symbol to "가" for Hangul, "A" for English
        const isHangul = symbolText !== 'A' && symbolText !== 'EN' &&
                         symbolText !== 'En' && symbolText !== 'en';

        this._indicator.setHangulMode(isHangul);
    }

    /**
     * Hide GNOME Shell's default keyboard indicator to avoid duplication.
     * We only hide it when DKST is active.
     */
    _hideDefaultIndicator() {
        const indicator = Main.panel.statusArea['keyboard'];
        if (indicator) {
            this._originalIndicator = indicator;
            indicator.hide();
        }
    }

    /**
     * Restore GNOME Shell's default keyboard indicator.
     */
    _showDefaultIndicator() {
        if (this._originalIndicator) {
            this._originalIndicator.show();
            this._originalIndicator = null;
        } else {
            const indicator = Main.panel.statusArea['keyboard'];
            if (indicator)
                indicator.show();
        }
    }

    /**
     * Restore the GNOME Shell IBus input source indicator container.
     */
    _showIBusIndicatorContainer() {
        try {
            const inputSourceIndicator = Main.panel.statusArea['keyboard'];
            if (inputSourceIndicator?.container)
                inputSourceIndicator.container.visible = true;
        } catch (e) {
            console.error(`[DKST] Failed to restore IBus indicator: ${e.message}`);
        }
    }

    /**
     * Get GNOME Shell's internal IBusManager instance.
     */
    _getIBusManager() {
        try {
            // GNOME Shell's IBusManager is accessible via the keyboard indicator
            // or through the InputSourceManager
            const {IBusManager} = Main.panel.statusArea['keyboard']?._inputSourceManager?._ibusManager
                ? {IBusManager: Main.panel.statusArea['keyboard']._inputSourceManager._ibusManager}
                : {};

            if (IBusManager)
                return IBusManager;

            // Alternative: try to get it from the InputSourceManager directly
            const inputSourceManager = this._getInputSourceManager();
            if (inputSourceManager?._ibusManager)
                return inputSourceManager._ibusManager;

        } catch (e) {
            console.error(`[DKST] Failed to get IBusManager: ${e.message}`);
        }

        return null;
    }

    /**
     * Get GNOME Shell's InputSourceManager instance.
     */
    _getInputSourceManager() {
        try {
            const keyboard = Main.panel.statusArea['keyboard'];
            if (keyboard?._inputSourceManager)
                return keyboard._inputSourceManager;
        } catch (e) {
            console.error(`[DKST] Failed to get InputSourceManager: ${e.message}`);
        }
        return null;
    }
}
