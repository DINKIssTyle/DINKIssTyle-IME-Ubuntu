#!/usr/bin/env python3
# Created by DINKIssTyle on 2026. Copyright (C) 2026 DINKI'ssTyle. All rights reserved.
import argparse
import configparser
import os
import shutil
import signal
import subprocess
import tempfile
import urllib.error
import urllib.request

import gi

# Ensure IBus input method is used for Korean input inside the preferences UI.
os.environ.setdefault("GTK_IM_MODULE", "ibus")

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk, GdkPixbuf, Gtk

CONFIG_DIR = os.path.expanduser("~/.config/ibus-dkst")
CONFIG_FILE = os.path.join(CONFIG_DIR, "config.ini")
USER_DICT_FILE = os.path.join(CONFIG_DIR, "hanja_user.txt")
SYSTEM_DICT_FILE = "/usr/share/ibus-dkst/hanja.txt"
REMOTE_HANJA_URL = (
    "https://raw.githubusercontent.com/DINKIssTyle/"
    "DINKIssTyle-IME-macOS/main/Resources/hanja.txt"
)

APP_NAME = "DKST Linux(iBUS)용 한글입력기"
PREFS_VERSION = "1.1"

MACOS_KEYS = [
    ("y (ㅛ)", "Y"),
    ("u (ㅕ)", "U"),
    ("i (ㅑ)", "I"),
    ("a (ㅁ)", "A"),
    ("s (ㄴ)", "S"),
    ("d (ㅇ)", "D"),
    ("f (ㄹ)", "F"),
    ("g (ㅎ)", "G"),
    ("h (ㅗ)", "H"),
    ("j (ㅓ)", "J"),
    ("k (ㅏ)", "K"),
    ("l (ㅣ)", "L"),
    ("z (ㅋ)", "Z"),
    ("x (ㅌ)", "X"),
    ("c (ㅊ)", "C"),
    ("v (ㅍ)", "V"),
    ("b (ㅠ)", "B"),
    ("n (ㅜ)", "N"),
    ("m (ㅡ)", "M"),
]


class KeyCaptureDialog(Gtk.Dialog):
    def __init__(self, parent):
        super().__init__(title="단축키 입력", transient_for=parent, flags=0)
        self.add_buttons("취소", Gtk.ResponseType.CANCEL)
        self.set_default_size(320, 140)
        self.captured_key = None

        box = self.get_content_area()
        box.set_border_width(18)
        label = Gtk.Label(label="사용할 키 조합을 누르세요.")
        label.set_xalign(0.5)
        box.add(label)

        self.connect("key-press-event", self.on_key_press)
        self.show_all()

    def on_key_press(self, widget, event):
        if event.keyval in [
            Gdk.KEY_Shift_L,
            Gdk.KEY_Shift_R,
            Gdk.KEY_Control_L,
            Gdk.KEY_Control_R,
            Gdk.KEY_Alt_L,
            Gdk.KEY_Alt_R,
            Gdk.KEY_Meta_L,
            Gdk.KEY_Meta_R,
            Gdk.KEY_Super_L,
            Gdk.KEY_Super_R,
        ]:
            return False

        mods = []
        if event.state & Gdk.ModifierType.SHIFT_MASK:
            mods.append("Shift")
        if event.state & Gdk.ModifierType.CONTROL_MASK:
            mods.append("Control")
        if event.state & Gdk.ModifierType.MOD1_MASK:
            mods.append("Alt")
        if event.state & Gdk.ModifierType.SUPER_MASK:
            mods.append("Super")

        key_name = Gdk.keyval_name(event.keyval)
        self.captured_key = f"{'+'.join(mods)}+{key_name}" if mods else key_name
        self.response(Gtk.ResponseType.OK)
        return True


class SettingsWindow(Gtk.Window):
    def __init__(self, initial_tab=None):
        super().__init__(title="DKST Linux(iBUS)용 한글입력기 환경설정")
        self.set_border_width(0)
        self.set_default_size(720, 620)
        self.set_position(Gtk.WindowPosition.CENTER)

        self.config = configparser.ConfigParser()
        self.config.optionxform = str
        self.dictionary_entries = []

        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(outer)

        self.notebook = Gtk.Notebook()
        self.notebook.set_scrollable(True)
        outer.pack_start(self.notebook, True, True, 0)

        self._build_general_tab()
        self._build_mapping_tab()
        self._build_dictionary_tab()
        self._build_about_tab()
        self._build_footer(outer)

        self.load_config()
        self.load_dictionary()
        self.select_initial_tab(initial_tab)

    def _page_label(self, text):
        label = Gtk.Label(label=text)
        label.set_margin_start(12)
        label.set_margin_end(12)
        return label

    def _section(self, title):
        frame = Gtk.Frame(label=title)
        frame.set_shadow_type(Gtk.ShadowType.NONE)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_border_width(12)
        frame.add(box)
        return frame, box

    def _build_general_tab(self):
        page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        page.set_border_width(18)
        self.notebook.append_page(page, self._page_label("환경설정"))

        frame_basic, basic = self._section("입력")
        page.pack_start(frame_basic, False, False, 0)

        self.check_moa = Gtk.CheckButton(label="모아찍기 사용")
        basic.pack_start(self.check_moa, False, False, 0)

        self.check_indicator = Gtk.CheckButton(label="커서 언어 표시기 보이기 (한/A)")
        basic.pack_start(self.check_indicator, False, False, 0)

        row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        row.pack_start(Gtk.Label(label="백스페이스:"), False, False, 0)
        self.bs_jaso = Gtk.RadioButton.new_with_label_from_widget(None, "자소 단위")
        self.bs_char = Gtk.RadioButton.new_with_label_from_widget(self.bs_jaso, "글자 단위")
        row.pack_start(self.bs_jaso, False, False, 0)
        row.pack_start(self.bs_char, False, False, 0)
        basic.pack_start(row, False, False, 0)

        frame_toggle, toggle_box = self._section("한/영 전환 단축키")
        page.pack_start(frame_toggle, True, True, 0)
        self.toggle_store, self.toggle_tree = self._build_key_list(toggle_box)
        self._build_key_buttons(toggle_box, self.on_add_key, self.on_remove_key)

        frame_hanja, hanja_box = self._section("한자/사전 변환 단축키")
        page.pack_start(frame_hanja, True, True, 0)
        self.hanja_store, self.hanja_tree = self._build_key_list(hanja_box)
        self._build_key_buttons(hanja_box, self.on_add_hanja_key, self.on_remove_hanja_key)

    def _build_key_list(self, parent):
        body = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        parent.pack_start(body, True, True, 0)

        store = Gtk.ListStore(str)
        tree = Gtk.TreeView(model=store)
        tree.set_headers_visible(False)
        tree.append_column(Gtk.TreeViewColumn("Key", Gtk.CellRendererText(), text=0))

        scroll = Gtk.ScrolledWindow()
        scroll.set_min_content_height(96)
        scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        scroll.add(tree)
        body.pack_start(scroll, True, True, 0)
        return store, tree

    def _build_key_buttons(self, parent, add_cb, remove_cb):
        body = parent.get_children()[-1]
        buttons = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        body.pack_start(buttons, False, False, 0)

        btn_add = Gtk.Button(label="추가")
        btn_add.connect("clicked", add_cb)
        buttons.pack_start(btn_add, False, False, 0)

        btn_remove = Gtk.Button(label="삭제")
        btn_remove.connect("clicked", remove_cb)
        buttons.pack_start(btn_remove, False, False, 0)

    def _build_mapping_tab(self):
        page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        page.set_border_width(18)
        self.notebook.append_page(page, self._page_label("단모음/단자음"))

        self.check_custom = Gtk.CheckButton(label="Shift + 단자음/단모음 사용자화 사용")
        self.check_custom.connect("toggled", self.on_custom_toggled)
        page.pack_start(self.check_custom, False, False, 0)

        info = Gtk.Label(
            label="Shift 키와 함께 입력할 때 대신 출력할 문자나 문구를 설정합니다."
        )
        info.set_xalign(0)
        page.pack_start(info, False, False, 0)

        self.mapping_store = Gtk.ListStore(str, str, str)
        for display, key in MACOS_KEYS:
            self.mapping_store.append([display, "", key])

        self.mapping_tree = Gtk.TreeView(model=self.mapping_store)
        self.mapping_tree.set_grid_lines(Gtk.TreeViewGridLines.BOTH)

        col_key = Gtk.TreeViewColumn("키", Gtk.CellRendererText(), text=0)
        col_key.set_min_width(120)
        self.mapping_tree.append_column(col_key)

        renderer_val = Gtk.CellRendererText()
        renderer_val.set_property("editable", True)
        renderer_val.connect("edited", self.on_mapping_edited)
        col_val = Gtk.TreeViewColumn("출력 내용", renderer_val, text=1)
        col_val.set_expand(True)
        self.mapping_tree.append_column(col_val)

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        scroll.add(self.mapping_tree)
        page.pack_start(scroll, True, True, 0)

    def _build_dictionary_tab(self):
        page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        page.set_border_width(18)
        self.notebook.append_page(page, self._page_label("사전"))

        top = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        page.pack_start(top, False, False, 0)

        self.search_entry = Gtk.SearchEntry()
        self.search_entry.set_placeholder_text("한글 또는 후보 문자를 검색")
        self.search_entry.connect("search-changed", self.on_dictionary_search_changed)
        top.pack_start(self.search_entry, True, True, 0)

        btn_update = Gtk.Button(label="온라인 업데이트")
        btn_update.connect("clicked", self.on_update_dictionary_clicked)
        top.pack_start(btn_update, False, False, 0)

        btn_refresh = Gtk.Button(label="새로고침")
        btn_refresh.connect("clicked", self.on_refresh_dictionary_clicked)
        top.pack_start(btn_refresh, False, False, 0)

        note = Gtk.Label(
            label=(
                "사용자 사전은 ~/.config/ibus-dkst/hanja_user.txt에 저장됩니다. "
                "온라인 업데이트는 macOS 저장소의 최신 hanja.txt를 시스템 사전에 반영합니다."
            )
        )
        note.set_xalign(0)
        note.set_line_wrap(True)
        page.pack_start(note, False, False, 0)

        self.dict_store = Gtk.ListStore(str, str, int)  # Key, Values, Source (0:System, 1:User)
        
        self.dict_filter = self.dict_store.filter_new()
        self.dict_filter.set_visible_func(self.filter_dictionary_func)
        
        self.dict_tree = Gtk.TreeView(model=self.dict_filter)
        self.dict_tree.set_grid_lines(Gtk.TreeViewGridLines.BOTH)

        renderer_key = Gtk.CellRendererText()
        renderer_key.set_property("editable", True)
        renderer_key.connect("edited", self.on_dict_key_edited)
        col_key = Gtk.TreeViewColumn("문자", renderer_key, text=0)
        col_key.set_min_width(130)
        self.dict_tree.append_column(col_key)

        renderer_val = Gtk.CellRendererText()
        renderer_val.set_property("editable", True)
        renderer_val.connect("edited", self.on_dict_value_edited)
        col_val = Gtk.TreeViewColumn("한자 후보창에 보여줄 문자", renderer_val, text=1)
        col_val.set_expand(True)
        self.dict_tree.append_column(col_val)

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        scroll.add(self.dict_tree)
        page.pack_start(scroll, True, True, 0)

        bottom = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        page.pack_start(bottom, False, False, 0)

        btn_add = Gtk.Button(label="+ 추가")
        btn_add.connect("clicked", self.on_add_dictionary_entry)
        bottom.pack_start(btn_add, False, False, 0)

        btn_remove = Gtk.Button(label="- 제거")
        btn_remove.connect("clicked", self.on_remove_dictionary_entry)
        bottom.pack_start(btn_remove, False, False, 0)

        bottom.pack_start(Gtk.Box(), True, True, 0)

        btn_open_user = Gtk.Button(label="사용자 사전 파일 열기")
        btn_open_user.connect("clicked", self.on_open_user_dictionary)
        bottom.pack_end(btn_open_user, False, False, 0)

        btn_open_system = Gtk.Button(label="시스템 사전 파일 열기")
        btn_open_system.connect("clicked", self.on_open_system_dictionary)
        bottom.pack_end(btn_open_system, False, False, 0)

    def _build_about_tab(self):
        page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        page.set_border_width(28)
        page.set_halign(Gtk.Align.CENTER)
        self.notebook.append_page(page, self._page_label("정보"))

        script_dir = os.path.dirname(os.path.realpath(__file__))
        icon_path = os.path.join(script_dir, "icon.png")
        if os.path.exists(icon_path):
            try:
                pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(icon_path, 96, 96, True)
                page.pack_start(Gtk.Image.new_from_pixbuf(pixbuf), False, False, 0)
            except Exception:
                pass

        title = Gtk.Label()
        title.set_markup(f"<span size='x-large' weight='bold'>{APP_NAME}</span>")
        page.pack_start(title, False, False, 0)

        desc = Gtk.Label(label="Korean Input Method and Utilities")
        page.pack_start(desc, False, False, 0)

        version = Gtk.Label(label=f"환경설정 버전: {PREFS_VERSION}")
        page.pack_start(version, False, False, 0)

        paths = Gtk.Label(
            label=(
                f"설정 파일: {CONFIG_FILE}\n"
                f"사용자 사전: {USER_DICT_FILE}\n"
                f"시스템 사전: {SYSTEM_DICT_FILE}"
            )
        )
        paths.set_selectable(True)
        paths.set_justify(Gtk.Justification.CENTER)
        page.pack_start(paths, False, False, 0)

        copy = Gtk.Label(label="(C) 2025-2026 DINKI'ssTyle")
        page.pack_start(copy, False, False, 0)

    def _build_footer(self, outer):
        footer = Gtk.ButtonBox(orientation=Gtk.Orientation.HORIZONTAL)
        footer.set_border_width(10)
        footer.set_layout(Gtk.ButtonBoxStyle.END)
        outer.pack_start(footer, False, False, 0)

        btn_cancel = Gtk.Button(label="닫기")
        btn_cancel.connect("clicked", Gtk.main_quit)
        footer.add(btn_cancel)

        btn_apply = Gtk.Button(label="적용")
        btn_apply.connect("clicked", self.on_apply)
        footer.add(btn_apply)

        btn_ok = Gtk.Button(label="확인")
        btn_ok.connect("clicked", self.on_ok)
        footer.add(btn_ok)

    def select_initial_tab(self, initial_tab):
        tab_map = {
            "general": 0,
            "settings": 0,
            "mappings": 1,
            "mapping": 1,
            "dictionary": 2,
            "dict": 2,
            "about": 3,
            "info": 3,
        }
        if initial_tab in tab_map:
            self.notebook.set_current_page(tab_map[initial_tab])

    def load_config(self):
        is_moa = False
        is_indicator = True
        bs_mode = "JASO"
        is_custom = False
        toggle_keys_str = "Shift+space;Hangul"
        hanja_keys_str = "Alt+Return;Hangul_Hanja"

        if os.path.exists(CONFIG_FILE):
            try:
                self.config.read(CONFIG_FILE)
                if "Settings" in self.config:
                    is_moa = self.config.getboolean(
                        "Settings", "EnableMoaJjiki", fallback=False
                    )
                    is_indicator = self.config.getboolean(
                        "Settings", "EnableIndicator", fallback=True
                    )
                    bs_mode = self.config.get("Settings", "BackspaceMode", fallback="JASO")
                    is_custom = self.config.getboolean(
                        "Settings", "EnableCustomShift", fallback=False
                    )

                if "ToggleKeys" in self.config and "Keys" in self.config["ToggleKeys"]:
                    toggle_keys_str = self.config["ToggleKeys"]["Keys"]

                if "HanjaKeys" in self.config and "Keys" in self.config["HanjaKeys"]:
                    hanja_keys_str = self.config["HanjaKeys"]["Keys"]

                if "CustomShift" in self.config:
                    for row in self.mapping_store:
                        conf_key = row[2]
                        if conf_key in self.config["CustomShift"]:
                            row[1] = self.config["CustomShift"][conf_key]
            except Exception as exc:
                self.show_error("설정 파일을 읽을 수 없습니다.", str(exc))

        self.check_moa.set_active(is_moa)
        self.check_indicator.set_active(is_indicator)
        self.bs_char.set_active(bs_mode == "CHAR")
        self.bs_jaso.set_active(bs_mode != "CHAR")
        self.check_custom.set_active(is_custom)
        self.mapping_tree.set_sensitive(is_custom)

        self.toggle_store.clear()
        for key in filter(None, (k.strip() for k in toggle_keys_str.split(";"))):
            self.toggle_store.append([key])

        self.hanja_store.clear()
        for key in filter(None, (k.strip() for k in hanja_keys_str.split(";"))):
            self.hanja_store.append([key])

    def save_to_config(self):
        if "Settings" not in self.config:
            self.config["Settings"] = {}

        self.config["Settings"]["EnableMoaJjiki"] = (
            "true" if self.check_moa.get_active() else "false"
        )
        self.config["Settings"]["EnableIndicator"] = (
            "true" if self.check_indicator.get_active() else "false"
        )
        self.config["Settings"]["BackspaceMode"] = (
            "CHAR" if self.bs_char.get_active() else "JASO"
        )
        self.config["Settings"]["EnableCustomShift"] = (
            "true" if self.check_custom.get_active() else "false"
        )

        if "ToggleKeys" not in self.config:
            self.config["ToggleKeys"] = {}
        self.config["ToggleKeys"]["Keys"] = ";".join(row[0] for row in self.toggle_store)

        if "HanjaKeys" not in self.config:
            self.config["HanjaKeys"] = {}
        self.config["HanjaKeys"]["Keys"] = ";".join(row[0] for row in self.hanja_store)

        if "CustomShift" in self.config:
            self.config.remove_section("CustomShift")
        self.config.add_section("CustomShift")
        for row in self.mapping_store:
            key = row[2]
            val = row[1].strip()
            if key and val:
                self.config["CustomShift"][key] = val

        os.makedirs(CONFIG_DIR, exist_ok=True)
        with open(CONFIG_FILE, "w", encoding="utf-8") as file:
            self.config.write(file)

    def load_dictionary(self):
        self.dict_store.clear()
        
        # Load System Dictionary (Source 0)
        if os.path.exists(SYSTEM_DICT_FILE):
            try:
                with open(SYSTEM_DICT_FILE, "r", encoding="utf-8") as file:
                    for line in file:
                        line = line.strip()
                        if line and ":" in line and not line.startswith("#") and "###DKST" not in line:
                            key, values = line.split(":", 1)
                            self.dict_store.append([key.strip(), values.strip(), 0])
            except Exception as exc:
                print(f"System dict load error: {exc}")

        # Load User Dictionary (Source 1)
        if os.path.exists(USER_DICT_FILE):
            try:
                with open(USER_DICT_FILE, "r", encoding="utf-8") as file:
                    for line in file:
                        line = line.strip()
                        if line and ":" in line and not line.startswith("#"):
                            key, values = line.split(":", 1)
                            # If a user entry exists for the same key, it might be better to 
                            # keep them both or show priority. Here we just add them all.
                            self.dict_store.append([key.strip(), values.strip(), 1])
            except Exception as exc:
                print(f"User dict load error: {exc}")
        
        self.refresh_dictionary_store()

    def refresh_dictionary_store(self):
        self.dict_filter.refilter()

    def filter_dictionary_func(self, model, iter, data):
        query = self.search_entry.get_text().strip().lower()
        if not query:
            return True
        key = model[iter][0].lower()
        values = model[iter][1].lower()
        return query in key or query in values

    def save_dictionary(self):
        system_lines = ["# DKST System Hanja Dictionary\n", "# Format: hangul:hanja1,hanja2,...\n\n"]
        user_lines = ["# DKST User Hanja Dictionary\n", "# Format: hangul:hanja1,hanja2,...\n\n"]
        
        for row in self.dict_store:
            key = row[0].replace(":", "").strip()
            values = self.clean_candidates(row[1])
            source = row[2]
            if not key or not values:
                continue
            line = f"{key}:{values}\n"
            if source == 0:
                system_lines.append(line)
            else:
                user_lines.append(line)

        # Save User Dictionary
        os.makedirs(CONFIG_DIR, exist_ok=True)
        user_content = "".join(user_lines)
        with open(USER_DICT_FILE, "w", encoding="utf-8") as file:
            file.write(user_content)

        # Save System Dictionary only if changed
        new_system_content = "".join(system_lines)
        old_system_content = ""
        if os.path.exists(SYSTEM_DICT_FILE):
            try:
                with open(SYSTEM_DICT_FILE, "r", encoding="utf-8") as file:
                    old_system_content = file.read()
            except Exception:
                pass

        if new_system_content.strip() != old_system_content.strip():
            with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", delete=False) as tmp:
                tmp.write(new_system_content)
                tmp_path = tmp.name

            try:
                shutil.copyfile(tmp_path, SYSTEM_DICT_FILE)
                os.chmod(SYSTEM_DICT_FILE, 0o644)
            except PermissionError:
                # Combine commands into one pkexec call to prompt only once
                command = f"cp {tmp_path} {SYSTEM_DICT_FILE} && chmod 644 {SYSTEM_DICT_FILE}"
                subprocess.run(["pkexec", "sh", "-c", command], check=True)
            finally:
                if os.path.exists(tmp_path):
                    os.unlink(tmp_path)

    def clean_candidates(self, text):
        values = []
        seen = set()
        for part in text.replace(" ,", ",").replace(", ", ",").split(","):
            item = part.strip()
            if item and item not in seen:
                values.append(item)
                seen.add(item)
        return ",".join(values)

    def update_system_dictionary(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            remote_path = os.path.join(temp_dir, "hanja.txt")
            
            # Use a User-Agent to avoid being blocked by some servers
            req = urllib.request.Request(
                REMOTE_HANJA_URL, headers={"User-Agent": "DKST-Settings/1.1"}
            )
            try:
                with urllib.request.urlopen(req) as response:
                    with open(remote_path, "wb") as file:
                        file.write(response.read())
            except Exception as exc:
                raise RuntimeError(f"다운로드 중 오류 발생: {exc}")

            if not os.path.exists(remote_path) or not os.path.getsize(remote_path):
                raise RuntimeError("다운로드한 사전 파일이 비어 있습니다.")

            with open(remote_path, "r", encoding="utf-8") as file:
                remote_lines = file.read().splitlines()
            
            # Remove macOS user part if present (separator: ###DKST)
            for i, line in enumerate(remote_lines):
                if "###DKST" in line:
                    remote_lines = remote_lines[:i]
                    break
            
            with open(remote_path, "w", encoding="utf-8") as file:
                file.write("\n".join(remote_lines).rstrip() + "\n")

            # Copy to system path
            try:
                os.makedirs(os.path.dirname(SYSTEM_DICT_FILE), exist_ok=True)
                shutil.copyfile(remote_path, SYSTEM_DICT_FILE)
            except PermissionError:
                # Use pkexec for root permission
                # We also set permissions to 644 to ensure it's readable
                copy_cmd = ["pkexec", "cp", remote_path, SYSTEM_DICT_FILE]
                chmod_cmd = ["pkexec", "chmod", "644", SYSTEM_DICT_FILE]
                
                result = subprocess.run(copy_cmd, text=True, capture_output=True, check=False)
                if result.returncode != 0:
                    detail = result.stderr.strip() or result.stdout.strip()
                    raise RuntimeError(detail or "관리자 권한 복사에 실패했습니다.")
                
                subprocess.run(chmod_cmd, check=False)

    def restart_ibus(self):
        try:
            subprocess.run(["ibus", "restart"], check=False)
        except Exception:
            pass

    def on_add_key(self, widget):
        self.capture_key_into_store(self.toggle_store)

    def on_remove_key(self, widget):
        self.remove_selected_row(self.toggle_tree)

    def on_add_hanja_key(self, widget):
        self.capture_key_into_store(self.hanja_store)

    def on_remove_hanja_key(self, widget):
        self.remove_selected_row(self.hanja_tree)

    def capture_key_into_store(self, store):
        dialog = KeyCaptureDialog(self)
        response = dialog.run()
        if response == Gtk.ResponseType.OK and dialog.captured_key:
            if not any(row[0] == dialog.captured_key for row in store):
                store.append([dialog.captured_key])
        dialog.destroy()

    def remove_selected_row(self, tree):
        selection = tree.get_selection()
        model, iterator = selection.get_selected()
        if iterator:
            model.remove(iterator)

    def on_custom_toggled(self, widget):
        self.mapping_tree.set_sensitive(widget.get_active())

    def on_mapping_edited(self, widget, path, new_text):
        self.mapping_store[path][1] = new_text

    def on_dictionary_search_changed(self, widget):
        self.refresh_dictionary_store()

    def on_dict_key_edited(self, widget, path, new_text):
        clean_key = new_text.replace(":", "").strip()
        # Convert filter path to child path
        child_path = self.dict_filter.convert_path_to_child_path(Gtk.TreePath(path))
        self.dict_store[child_path][0] = clean_key

    def on_dict_value_edited(self, widget, path, new_text):
        clean_values = self.clean_candidates(new_text)
        child_path = self.dict_filter.convert_path_to_child_path(Gtk.TreePath(path))
        self.dict_store[child_path][1] = clean_values

    def on_add_dictionary_entry(self, widget):
        # Always add to User Dictionary (Source 1)
        self.dict_store.append(["", "", 1])
        path = Gtk.TreePath(len(self.dict_store) - 1)
        self.dict_tree.set_cursor(path, self.dict_tree.get_column(0), True)

    def on_remove_dictionary_entry(self, widget):
        selection = self.dict_tree.get_selection()
        model, iterator = selection.get_selected()
        if iterator:
            child_iter = self.dict_filter.convert_iter_to_child_iter(iterator)
            self.dict_store.remove(child_iter)

    def on_refresh_dictionary_clicked(self, widget):
        try:
            self.load_dictionary()
            self.show_info("새로고침 완료", "사전 데이터를 다시 불러왔습니다.")
        except Exception as exc:
            self.show_error("새로고침 실패", str(exc))

    def on_update_dictionary_clicked(self, widget):
        dialog = Gtk.MessageDialog(
            transient_for=self,
            flags=0,
            message_type=Gtk.MessageType.QUESTION,
            buttons=Gtk.ButtonsType.OK_CANCEL,
            text="시스템 한자 사전을 온라인 업데이트할까요?",
        )
        dialog.format_secondary_text(
            "macOS 저장소의 최신 Resources/hanja.txt를 내려받아 "
            "/usr/share/ibus-dkst/hanja.txt에 반영합니다."
        )
        response = dialog.run()
        dialog.destroy()
        if response != Gtk.ResponseType.OK:
            return

        try:
            self.update_system_dictionary()
            self.restart_ibus()
            self.show_info("업데이트 완료", "한자 사전이 업데이트되고 IBus가 재시작되었습니다.")
        except urllib.error.URLError as exc:
            self.show_error("사전을 다운로드할 수 없습니다.", str(exc))
        except Exception as exc:
            self.show_error("사전 업데이트에 실패했습니다.", str(exc))

    def on_open_user_dictionary(self, widget):
        os.makedirs(CONFIG_DIR, exist_ok=True)
        if not os.path.exists(USER_DICT_FILE):
            self.save_dictionary()
        self.open_path(USER_DICT_FILE)

    def on_open_system_dictionary(self, widget):
        self.open_path(SYSTEM_DICT_FILE)

    def open_path(self, path):
        try:
            subprocess.Popen(["xdg-open", path])
        except Exception as exc:
            self.show_error("파일을 열 수 없습니다.", str(exc))

    def on_apply(self, widget):
        try:
            self.save_to_config()
            self.save_dictionary()
            self.restart_ibus()
            self.show_info("적용 완료", "설정과 사용자 사전이 저장되었습니다.")
        except Exception as exc:
            self.show_error("저장할 수 없습니다.", str(exc))

    def on_ok(self, widget):
        try:
            self.save_to_config()
            self.save_dictionary()
            self.restart_ibus()
            Gtk.main_quit()
        except Exception as exc:
            self.show_error("저장할 수 없습니다.", str(exc))

    def show_info(self, title, detail):
        dialog = Gtk.MessageDialog(
            transient_for=self,
            flags=0,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text=title,
        )
        dialog.format_secondary_text(detail)
        dialog.run()
        dialog.destroy()

    def show_error(self, title, detail):
        dialog = Gtk.MessageDialog(
            transient_for=self,
            flags=0,
            message_type=Gtk.MessageType.ERROR,
            buttons=Gtk.ButtonsType.OK,
            text=title,
        )
        dialog.format_secondary_text(detail)
        dialog.run()
        dialog.destroy()


def main():
    parser = argparse.ArgumentParser(description="DKST 환경설정")
    parser.add_argument(
        "--tab",
        choices=["general", "settings", "mappings", "mapping", "dictionary", "dict", "about", "info"],
        help="처음 열 탭",
    )
    args = parser.parse_args()

    win = SettingsWindow(initial_tab=args.tab)
    win.connect("destroy", Gtk.main_quit)
    win.show_all()

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    Gtk.main()


if __name__ == "__main__":
    main()
