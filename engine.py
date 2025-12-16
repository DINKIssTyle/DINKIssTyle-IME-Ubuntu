import sys
import os
import json
import gi
gi.require_version('IBus', '1.0')
from gi.repository import IBus, GLib

from hangul import DKSTHangul

CONFIG_PATH = os.path.expanduser("~/.config/dinkisstyle/config.json")

class DKSTEngine(IBus.Engine):
    def __init__(self):
        super().__init__()
        self.hangul = DKSTHangul()
        self.lookup_table = IBus.LookupTable.new(10, 0, True, True)
        self.config = self.load_config()
        self.apply_config()
        
    def load_config(self):
        default_config = {
            "EnableMoaJjiki": True,
            "EnableCustomShift": False,
            "CustomShiftMappings": {}
        }
        if os.path.exists(CONFIG_PATH):
            try:
                with open(CONFIG_PATH, 'r') as f:
                    data = json.load(f)
                    default_config.update(data)
            except:
                pass
        return default_config

    def apply_config(self):
        self.hangul.moa_jjiki_enabled = self.config.get("EnableMoaJjiki", True)
    
    def do_process_key_event(self, keyval, keycode, state):
        # Ignore key releases
        if state & IBus.ModifierType.RELEASE_MASK:
            return False
            
        # Check Custom Shift Mappings: Shift + Key
        # Only if Shift is ON and others (Ctrl/Alt/Super) are OFF
        is_shift = (state & IBus.ModifierType.SHIFT_MASK) and not (state & (IBus.ModifierType.CONTROL_MASK | IBus.ModifierType.MOD1_MASK | IBus.ModifierType.SUPER_MASK))
        
        if self.config.get("EnableCustomShift", False) and is_shift:
            # Map keyval to character for lookup
            # In Linux, keyval for Shift+A is usually 'A'.
            if 32 <= keyval <= 126:
                char_key = chr(keyval) # e.g. 'A', 'Q', etc.
                mappings = self.config.get("CustomShiftMappings", {})
                
                # The Mac mappings use specific string keys like "y (ㅛ)", we might need a simpler lookup or migration.
                # Assuming the config uses simple keys like "Q" or "W".
                if char_key in mappings:
                    self.commit() # Flush current hangul
                    
                    mapped_str = mappings[char_key]
                    text = IBus.Text.new_from_string(mapped_str)
                    self.commit_text(text)
                    return True

        # Modifiers check (Ctrl, Alt, Super/Command shouldn't be processed for Hangul)
        if state & (IBus.ModifierType.CONTROL_MASK | IBus.ModifierType.MOD1_MASK | IBus.ModifierType.SUPER_MASK):
            return False

        # Backspace
        if keyval == IBus.KEY_BackSpace:
            if self.hangul.backspace():
                self.update_preedit()
                return True
            return False # Let system handle if nothing to backspace
            
        # Space or Enter
        if keyval == IBus.KEY_space or keyval == IBus.KEY_Return:
            self.commit()
            return False # Let system handle the actual space/enter insertion
            
        # Printable characters common range
        if 32 <= keyval <= 126:
            char = chr(keyval)
            # engine receives shifted chars (e.g. 'A'). Our hangul logic handles 'q' vs 'Q' properly.
            if self.hangul.process_code(char):
                self.commit_pending() # Commit any pending completed/commit strings from hangul
                self.update_preedit()
                return True
        
        # If we have composed text but type something else (special key?), commit and pass through
        if self.hangul.composed_string():
            self.commit()
            return False
            
        return False

    def update_preedit(self):
        composed = self.hangul.composed_string()
        if composed:
            text = IBus.Text.new_from_string(composed)
            text.set_attributes(IBus.AttrList())
            text.append_attribute(IBus.Attribute.Type.UNDERLINE, 1, 0, len(composed))
            self.update_preedit_text(text, len(composed), True)
        else:
            self.hide_preedit_text()

    def commit_pending(self):
        # If engine has 'commit_string' ready
        commit_str = self.hangul.commit_string()
        if commit_str:
            text = IBus.Text.new_from_string(commit_str)
            self.commit_text(text)

    def commit(self):
        # Force flush current composition
        composed = self.hangul.composed_string()
        commit_str = self.hangul.commit_string() # This clears buffer
        
        full_text = commit_str + composed
        if full_text:
            text = IBus.Text.new_from_string(full_text)
            self.commit_text(text)
        
        self.hangul.reset()
        self.hide_preedit_text()
        
    def do_focus_in(self):
        self.register_properties(IBus.PropList())
        # Reload config on focus in just in case? Maybe overkill.
        # self.config = self.load_config()
        # self.apply_config()

    def do_focus_out(self):
        self.commit()

    def do_reset(self):
        self.commit()
        self.hangul.reset()

def main():
    bus = IBus.Bus()
    bus.request_name("com.dinkisstyle.inputmethod", 0)
    
    factory = IBus.Factory.new(bus.get_connection())
    factory.add_engine("dinkisstyle", DKSTEngine)
    
    main_loop = GLib.MainLoop()
    main_loop.run()

if __name__ == "__main__":
    main()
