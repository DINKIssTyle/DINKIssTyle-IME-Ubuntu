#!/usr/bin/env python3
import gi
import configparser
import os
import sys
import signal

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, GdkPixbuf

CONFIG_DIR = os.path.expanduser("~/.config/ibus-dkst")
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

class KeyCaptureDialog(Gtk.Dialog):
    def __init__(self, parent):
        super().__init__(title="Input Key", transient_for=parent, flags=0)
        self.add_buttons("Cancel", Gtk.ResponseType.CANCEL)
        
        self.set_default_size(300, 150)
        self.captured_key = None
        self.captured_mod = 0
        
        box = self.get_content_area()
        self.label = Gtk.Label(label="Press any key combination...")
        box.add(self.label)
        
        self.connect("key-press-event", self.on_key_press)
        self.show_all()

    def on_key_press(self, widget, event):
        # Ignore standalone modifiers
        if event.keyval in [
            Gdk.KEY_Shift_L, Gdk.KEY_Shift_R, 
            Gdk.KEY_Control_L, Gdk.KEY_Control_R,
            Gdk.KEY_Alt_L, Gdk.KEY_Alt_R,
            Gdk.KEY_Meta_L, Gdk.KEY_Meta_R,
            Gdk.KEY_Super_L, Gdk.KEY_Super_R
        ]:
            return False
            
        # Modifiers
        mods = []
        if event.state & Gdk.ModifierType.SHIFT_MASK:
            mods.append("Shift")
        if event.state & Gdk.ModifierType.CONTROL_MASK:
            mods.append("Control")
        if event.state & Gdk.ModifierType.MOD1_MASK: # Alt
            mods.append("Alt")
        if event.state & Gdk.ModifierType.SUPER_MASK:
            mods.append("Super")

        key_name = Gdk.keyval_name(event.keyval)
        
        # Format: Modifier+Key
        if mods:
            self.captured_key = f"{'+'.join(mods)}+{key_name}"
        else:
            self.captured_key = key_name
            
        self.response(Gtk.ResponseType.OK)
        return True

class SettingsWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="DKST Settings")
        self.set_border_width(10)
        self.set_default_size(500, 700)
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

        # Indicator
        self.check_indicator = Gtk.CheckButton(label="Show Language Indicator (한/EN)")
        vbox_gen.pack_start(self.check_indicator, False, False, 0)
        
        # Backspace Mode
        hbox_bs = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        vbox_gen.pack_start(hbox_bs, False, False, 0)
        hbox_bs.pack_start(Gtk.Label(label="Backspace Unit:"), False, False, 0)
        
        self.bs_char = Gtk.RadioButton.new_with_label_from_widget(None, "Character (글자 단위)")
        self.bs_jaso = Gtk.RadioButton.new_with_label_from_widget(self.bs_char, "Jaso (자소 단위)")
        hbox_bs.pack_start(self.bs_char, False, False, 0)
        hbox_bs.pack_start(self.bs_jaso, False, False, 0)

        # 2. Toggle Keys Frame
        frame_toggle = Gtk.Frame(label="Hangul Toggle Keys (한영전환)")
        main_vbox.pack_start(frame_toggle, False, False, 0)
        
        hbox_toggle = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        hbox_toggle.set_border_width(10)
        frame_toggle.add(hbox_toggle)
        
        # List
        self.toggle_store = Gtk.ListStore(str)
        self.toggle_tree = Gtk.TreeView(model=self.toggle_store)
        self.toggle_tree.append_column(Gtk.TreeViewColumn("Key", Gtk.CellRendererText(), text=0))
        
        scroll_toggle = Gtk.ScrolledWindow()
        scroll_toggle.set_min_content_height(100)
        scroll_toggle.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scroll_toggle.add(self.toggle_tree)
        hbox_toggle.pack_start(scroll_toggle, True, True, 0)
        
        # Buttons
        vbox_toggle_btns = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        hbox_toggle.pack_start(vbox_toggle_btns, False, False, 0)
        
        btn_add_key = Gtk.Button(label="Add")
        btn_add_key.connect("clicked", self.on_add_key)
        vbox_toggle_btns.pack_start(btn_add_key, False, False, 0)
        
        btn_remove_key = Gtk.Button(label="Remove")
        btn_remove_key.connect("clicked", self.on_remove_key)
        vbox_toggle_btns.pack_start(btn_remove_key, False, False, 0)

        # 3. Custom Shift Mappings Frame
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

        # Buttons Area
        hbox_bottom = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        main_vbox.pack_start(hbox_bottom, False, False, 0)

        # About Button (Left)
        btn_about = Gtk.Button(label="About")
        btn_about.connect("clicked", self.on_about_clicked)
        hbox_bottom.pack_start(btn_about, False, False, 0)

        # Action Buttons (Right)
        bbox = Gtk.ButtonBox(orientation=Gtk.Orientation.HORIZONTAL)
        bbox.set_layout(Gtk.ButtonBoxStyle.END)
        hbox_bottom.pack_end(bbox, True, True, 0)
        
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
        is_indicator = True
        bs_mode = "JASO"
        is_custom = False
        toggle_keys_str = "Shift+space;Hangul"

        if os.path.exists(CONFIG_FILE):
            try:
                self.config.read(CONFIG_FILE)
                if "Settings" in self.config:
                    is_moa = self.config.getboolean("Settings", "EnableMoaJjiki", fallback=False)
                    is_indicator = self.config.getboolean("Settings", "EnableIndicator", fallback=True)
                    bs_mode = self.config.get("Settings", "BackspaceMode", fallback="JASO")
                    is_custom = self.config.getboolean("Settings", "EnableCustomShift", fallback=False)
                
                if "ToggleKeys" in self.config and "Keys" in self.config["ToggleKeys"]:
                    toggle_keys_str = self.config["ToggleKeys"]["Keys"]
                
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
        self.check_indicator.set_active(is_indicator)
        if bs_mode == "CHAR":
            self.bs_char.set_active(True)
        else:
            self.bs_jaso.set_active(True)
        self.check_custom.set_active(is_custom)
        self.tree.set_sensitive(is_custom)
        
        # Populate Toggle Keys
        self.toggle_store.clear()
        if toggle_keys_str:
            for key in toggle_keys_str.split(";"):
                if key.strip():
                    self.toggle_store.append([key.strip()])

    def save_to_config(self):
        if "Settings" not in self.config:
            self.config["Settings"] = {}
            
        # Use lowercase strings for GLib compatibility
        self.config["Settings"]["EnableMoaJjiki"] = "true" if self.check_moa.get_active() else "false"
        self.config["Settings"]["EnableIndicator"] = "true" if self.check_indicator.get_active() else "false"
        self.config["Settings"]["BackspaceMode"] = "CHAR" if self.bs_char.get_active() else "JASO"
        self.config["Settings"]["EnableCustomShift"] = "true" if self.check_custom.get_active() else "false"
        
        # Save Toggle Keys
        if "ToggleKeys" not in self.config:
            self.config["ToggleKeys"] = {}
        
        keys = []
        for row in self.toggle_store:
            keys.append(row[0])
        self.config["ToggleKeys"]["Keys"] = ";".join(keys)
        
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

    def on_add_key(self, widget):
        dlg = KeyCaptureDialog(self)
        resp = dlg.run()
        if resp == Gtk.ResponseType.OK:
            key = dlg.captured_key
            if key:
                # Check for duplicate
                exists = False
                for row in self.toggle_store:
                    if row[0] == key:
                        exists = True
                        break
                if not exists:
                    self.toggle_store.append([key])
        dlg.destroy()

    def on_remove_key(self, widget):
        selection = self.toggle_tree.get_selection()
        model, iter = selection.get_selected()
        if iter:
            model.remove(iter)

    def on_custom_toggled(self, widget):
        self.tree.set_sensitive(widget.get_active())

    def on_cell_edited(self, widget, path, new_text):
        self.store[path][1] = new_text

    def on_apply(self, widget):
        self.save_to_config()

    def on_ok(self, widget):
        self.save_to_config()
        Gtk.main_quit()

    def on_about_clicked(self, widget):
        dlg = Gtk.Dialog(title="DKST 정보", transient_for=self, flags=0)
        dlg.add_buttons("Close", Gtk.ResponseType.CLOSE)
        dlg.set_default_size(300, 250)
        
        box = dlg.get_content_area()
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_border_width(20)
        box.add(vbox)
        
        # Icon
        script_dir = os.path.dirname(os.path.realpath(__file__))
        icon_path = os.path.join(script_dir, "icon.png")
        if os.path.exists(icon_path):
            try:
                pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(icon_path, 64, 64, True)
                image = Gtk.Image.new_from_pixbuf(pixbuf)
                vbox.pack_start(image, False, False, 0)
            except Exception as e:
                print(f"Error loading icon: {e}")

        # Name
        lbl_name = Gtk.Label()
        lbl_name.set_markup("<span size='large' weight='bold'>DKST 한국어 입력기</span>")
        lbl_name.set_selectable(False)
        vbox.pack_start(lbl_name, False, False, 0)

        # Description
        lbl_desc = Gtk.Label(label="Korean Input Method and Utilities")
        lbl_desc.set_selectable(False)
        vbox.pack_start(lbl_desc, False, False, 0)
        
        # Copyright
        lbl_copy = Gtk.Label(label="(C) 2025 DINKI'ssTyle")
        lbl_copy.set_selectable(False)
        vbox.pack_start(lbl_copy, False, False, 0)

        dlg.show_all()
        dlg.run()
        dlg.destroy()

if __name__ == "__main__":
    win = SettingsWindow()
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    
    # Handle Ctrl+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    
    Gtk.main()
