#!/usr/bin/env python3
import gi
import configparser
import os
import sys
import signal

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk

CONFIG_DIR = os.path.expanduser("~/.config/ibus-dinkisstyle")
CONFIG_FILE = os.path.join(CONFIG_DIR, "config.ini")

# Key list from macOS PreferencesController.m
# Format: (Display Name, Config Key)
# We assume the user wants to map "Shift + Key".
# For standard US keyboard, Shift + y is Y.
MACOS_KEYS = [
    ("y (ㅛ)", "Y"), ("u (ㅕ)", "U"), ("i (ㅑ)", "I"),
    ("a (ㅁ)", "A"), ("s (ㄴ)", "S"), ("d (ㅇ)", "D"), ("f (ㄹ)", "F"), ("g (ㅎ)", "G"),
    ("h (ㅗ)", "H"), ("j (ㅓ)", "J"), ("k (ㅏ)", "K"), ("l (ㅣ)", "L"),
    ("z (ㅋ)", "Z"), ("x (ㅌ)", "X"), ("c (ㅊ)", "C"), ("v (ㅍ)", "V"), ("b (ㅠ)", "B"), ("n (ㅜ)", "N"), ("m (ㅡ)", "M")
]

class SettingsWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="DKST Settings")
        self.set_border_width(10)
        self.set_default_size(500, 600)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_modal(True)

        self.config = configparser.ConfigParser()
        self.config.optionxform = str # Preserve case for keys

        # Main Layout
        main_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.add(main_vbox)

        # 1. General Settings Frame
        frame_general = Gtk.Frame(label="General Settings")
        main_vbox.pack_start(frame_general, False, False, 0)
        
        vbox_gen = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        vbox_gen.set_border_width(10)
        frame_general.add(vbox_gen)

        # Moa-jjiki
        self.check_moa = Gtk.CheckButton(label="Enable Moa-jjiki (Combine Consonant+Vowel)")
        vbox_gen.pack_start(self.check_moa, False, False, 0)
        
        # Backspace Mode
        hbox_bs = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        vbox_gen.pack_start(hbox_bs, False, False, 0)
        hbox_bs.pack_start(Gtk.Label(label="Backspace Unit:"), False, False, 0)
        
        self.bs_char = Gtk.RadioButton.new_with_label_from_widget(None, "Character (글자 단위)")
        self.bs_jaso = Gtk.RadioButton.new_with_label_from_widget(self.bs_char, "Jaso (자소 단위)")
        hbox_bs.pack_start(self.bs_char, False, False, 0)
        hbox_bs.pack_start(self.bs_jaso, False, False, 0)

        # 2. Custom Shift Mappings Frame
        frame_custom = Gtk.Frame(label="Custom Shift Mappings")
        main_vbox.pack_start(frame_custom, True, True, 0)
        
        vbox_custom = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        vbox_custom.set_border_width(10)
        frame_custom.add(vbox_custom)

        # Enable Toggle
        self.check_custom = Gtk.CheckButton(label="Enable Custom Shift Shortcuts")
        self.check_custom.connect("toggled", self.on_custom_toggled)
        vbox_custom.pack_start(self.check_custom, False, False, 0)

        # List Structure: DisplayName, OutputValue, ConfigKey
        self.store = Gtk.ListStore(str, str, str)
        
        # Populate store
        for display, key in MACOS_KEYS:
            self.store.append([display, "", key])

        # TreeView
        self.tree = Gtk.TreeView(model=self.store)
        self.tree.set_grid_lines(Gtk.TreeViewGridLines.BOTH)
        
        # Column 1: Key (ReadOnly)
        renderer_key = Gtk.CellRendererText()
        col_key = Gtk.TreeViewColumn("Key", renderer_key, text=0)
        self.tree.append_column(col_key)
        
        # Column 2: Output (Editable)
        renderer_val = Gtk.CellRendererText()
        renderer_val.set_property("editable", True)
        renderer_val.connect("edited", self.on_cell_edited)
        col_val = Gtk.TreeViewColumn("Output (Text/Emoji)", renderer_val, text=1)
        self.tree.append_column(col_val)
        
        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scroll.add(self.tree)
        vbox_custom.pack_start(scroll, True, True, 0)

        # Buttons
        bbox = Gtk.ButtonBox(orientation=Gtk.Orientation.HORIZONTAL)
        bbox.set_layout(Gtk.ButtonBoxStyle.END)
        main_vbox.pack_start(bbox, False, False, 0)
        
        btn_cancel = Gtk.Button(label="Cancel")
        btn_cancel.connect("clicked", Gtk.main_quit)
        bbox.add(btn_cancel)
        
        btn_apply = Gtk.Button(label="Apply")
        btn_apply.connect("clicked", self.on_apply)
        bbox.add(btn_apply)
        
        btn_ok = Gtk.Button(label="OK")
        btn_ok.connect("clicked", self.on_ok)
        bbox.add(btn_ok)

        self.load_config()

    def load_config(self):
        # Default initialization
        is_moa = False
        bs_mode = "JASO"
        is_custom = False

        if os.path.exists(CONFIG_FILE):
            try:
                self.config.read(CONFIG_FILE)
                if "Settings" in self.config:
                    is_moa = self.config.getboolean("Settings", "EnableMoaJjiki", fallback=False)
                    bs_mode = self.config.get("Settings", "BackspaceMode", fallback="JASO")
                    is_custom = self.config.getboolean("Settings", "EnableCustomShift", fallback=False)
                
                if "CustomShift" in self.config:
                    # Update store from config
                    for row in self.store:
                        conf_key = row[2] # The 'Y', 'U' etc.
                        if conf_key in self.config["CustomShift"]:
                            row[1] = self.config["CustomShift"][conf_key]
            except Exception as e:
                print(f"Error loading config: {e}")

        # Set UI state
        self.check_moa.set_active(is_moa)
        if bs_mode == "CHAR":
            self.bs_char.set_active(True)
        else:
            self.bs_jaso.set_active(True)
        self.check_custom.set_active(is_custom)
        self.tree.set_sensitive(is_custom)

    def save_to_config(self):
        if "Settings" not in self.config:
            self.config["Settings"] = {}
            
        # Use lowercase strings for GLib compatibility
        self.config["Settings"]["EnableMoaJjiki"] = "true" if self.check_moa.get_active() else "false"
        self.config["Settings"]["BackspaceMode"] = "CHAR" if self.bs_char.get_active() else "JASO"
        self.config["Settings"]["EnableCustomShift"] = "true" if self.check_custom.get_active() else "false"
        
        # Save Mappings from Store
        if "CustomShift" in self.config:
            self.config.remove_section("CustomShift")
        self.config.add_section("CustomShift")
        
        for row in self.store:
            key = row[2] # Config Key (e.g., 'Y')
            val = row[1] # Output Value
            if key and val:
                self.config["CustomShift"][key] = val
        
        if not os.path.exists(CONFIG_DIR):
            os.makedirs(CONFIG_DIR)
        with open(CONFIG_FILE, "w") as f:
            self.config.write(f)

    def on_custom_toggled(self, widget):
        self.tree.set_sensitive(widget.get_active())

    def on_cell_edited(self, widget, path, new_text):
        self.store[path][1] = new_text

    def on_apply(self, widget):
        self.save_to_config()

    def on_ok(self, widget):
        self.save_to_config()
        Gtk.main_quit()

if __name__ == "__main__":
    win = SettingsWindow()
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    
    # Handle Ctrl+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    
    Gtk.main()
