#!/usr/bin/env python3
"""
DKST Hanja Dictionary Editor
Allows users to add, edit, and delete entries in their personal hanja dictionary.
"""
import gi
import os
import signal

# Ensure IBus input method is used for Korean input
os.environ.setdefault("GTK_IM_MODULE", "ibus")

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk

USER_CONFIG_DIR = os.path.expanduser("~/.config/ibus-dkst")
USER_DICT_FILE = os.path.join(USER_CONFIG_DIR, "hanja_user.txt")
SYSTEM_DICT_FILE = "/usr/share/ibus-dkst/hanja.txt"


class HanjaEditorWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="DKST 사전 편집기")
        self.set_border_width(10)
        self.set_default_size(600, 500)
        self.set_position(Gtk.WindowPosition.CENTER)

        # Main Layout
        main_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.add(main_vbox)

        # Info Label
        info_label = Gtk.Label()
        info_label.set_markup(
            "<b>사용자 사전</b>\n"
            "한글 단어에 대응하는 한자나 사용자 문구등을 추가할 수 있습니다.\n"
            "형식: 한글 → 한자 (뜻), 한자2 (뜻2), ..."
        )
        info_label.set_xalign(0)
        main_vbox.pack_start(info_label, False, False, 0)

        # List Store: Key (Hangul), Value (Hanja list)
        self.store = Gtk.ListStore(str, str)

        # TreeView
        self.tree = Gtk.TreeView(model=self.store)
        self.tree.set_grid_lines(Gtk.TreeViewGridLines.BOTH)

        # Column 1: Hangul (Editable)
        renderer_key = Gtk.CellRendererText()
        renderer_key.set_property("editable", True)
        renderer_key.connect("edited", self.on_key_edited)
        col_key = Gtk.TreeViewColumn("한글 (Key)", renderer_key, text=0)
        col_key.set_min_width(150)
        self.tree.append_column(col_key)

        # Column 2: Hanja (Editable)
        renderer_val = Gtk.CellRendererText()
        renderer_val.set_property("editable", True)
        renderer_val.connect("edited", self.on_value_edited)
        col_val = Gtk.TreeViewColumn("한자/사용자 문구 (Values)", renderer_val, text=1)
        col_val.set_expand(True)
        self.tree.append_column(col_val)

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        scroll.add(self.tree)
        main_vbox.pack_start(scroll, True, True, 0)

        # Button Row
        hbox_buttons = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        main_vbox.pack_start(hbox_buttons, False, False, 0)

        btn_add = Gtk.Button(label="추가")
        btn_add.connect("clicked", self.on_add_clicked)
        hbox_buttons.pack_start(btn_add, False, False, 0)

        btn_remove = Gtk.Button(label="삭제")
        btn_remove.connect("clicked", self.on_remove_clicked)
        hbox_buttons.pack_start(btn_remove, False, False, 0)

        # Spacer
        hbox_buttons.pack_start(Gtk.Box(), True, True, 0)

        btn_cancel = Gtk.Button(label="취소")
        btn_cancel.connect("clicked", Gtk.main_quit)
        hbox_buttons.pack_end(btn_cancel, False, False, 0)

        btn_save = Gtk.Button(label="저장")
        btn_save.connect("clicked", self.on_save_clicked)
        hbox_buttons.pack_end(btn_save, False, False, 0)

        # Load existing entries
        self.load_dictionary()

    def load_dictionary(self):
        """Load user dictionary file."""
        self.store.clear()

        if not os.path.exists(USER_DICT_FILE):
            return

        try:
            with open(USER_DICT_FILE, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    if ":" in line:
                        key, values = line.split(":", 1)
                        self.store.append([key.strip(), values.strip()])
        except Exception as e:
            print(f"Error loading dictionary: {e}")

    def save_dictionary(self):
        """Save user dictionary to file."""
        # Ensure directory exists
        if not os.path.exists(USER_CONFIG_DIR):
            os.makedirs(USER_CONFIG_DIR)

        try:
            with open(USER_DICT_FILE, "w", encoding="utf-8") as f:
                f.write("# DKST User Hanja Dictionary\n")
                f.write("# Format: hangul:hanja1 (meaning),hanja2 (meaning),...\n\n")
                for row in self.store:
                    key = row[0].strip()
                    values = row[1].strip()
                    if key and values:
                        f.write(f"{key}:{values}\n")
            return True
        except Exception as e:
            print(f"Error saving dictionary: {e}")
            return False

    def on_key_edited(self, widget, path, new_text):
        self.store[path][0] = new_text

    def on_value_edited(self, widget, path, new_text):
        self.store[path][1] = new_text

    def on_add_clicked(self, widget):
        self.store.append(["", ""])
        # Select the new row
        path = Gtk.TreePath(len(self.store) - 1)
        self.tree.set_cursor(path, self.tree.get_column(0), True)

    def on_remove_clicked(self, widget):
        selection = self.tree.get_selection()
        model, iter = selection.get_selected()
        if iter:
            model.remove(iter)

    def on_save_clicked(self, widget):
        if self.save_dictionary():
            # Restart IBus to reload dictionary
            import subprocess
            try:
                subprocess.run(["ibus", "restart"], check=False)
            except Exception:
                pass  # Ignore errors
            
            dialog = Gtk.MessageDialog(
                transient_for=self,
                flags=0,
                message_type=Gtk.MessageType.INFO,
                buttons=Gtk.ButtonsType.OK,
                text="저장 완료",
            )
            dialog.format_secondary_text(
                "사전이 저장되고 입력기가 재시작되었습니다."
            )
            dialog.run()
            dialog.destroy()


if __name__ == "__main__":
    win = HanjaEditorWindow()
    win.connect("destroy", Gtk.main_quit)
    win.show_all()

    # Handle Ctrl+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    Gtk.main()
