class DKSTHangul:
    def __init__(self):
        self._cho = 0
        self._jung = 0
        self._jong = 0
        self._buffer = []
        self._completed = []
        self.moa_jjiki_enabled = True

    def reset(self):
        self._cho = 0
        self._jung = 0
        self._jong = 0
        self._buffer = []
        self._completed = []

    def composed_string(self):
        if self._cho == 0 and self._jung == 0 and self._jong == 0:
            return ""
        return chr(self.current_syllable())

    def commit_string(self):
        res = "".join(self._completed)
        self._completed = []
        return res

    # Check types
    def is_cho(self, c): return 0x1100 <= c <= 0x1112
    def is_jung(self, c): return 0x1161 <= c <= 0x1175
    def is_jong(self, c): return 0x11A8 <= c <= 0x11C2

    def map_from_char(self, c):
        mapping = {
            'q': 0x1107, 'Q': 0x1108, # ㅂ, ㅃ
            'w': 0x110c, 'W': 0x110d, # ㅈ, ㅉ
            'e': 0x1103, 'E': 0x1104, # ㄷ, ㄸ
            'r': 0x1100, 'R': 0x1101, # ㄱ, ㄲ
            't': 0x1109, 'T': 0x110a, # ㅅ, ㅆ
            'y': 0x116d, 'Y': 0x116d, # ㅛ
            'u': 0x1167, 'U': 0x1167, # ㅕ
            'i': 0x1163, 'I': 0x1163, # ㅑ
            'o': 0x1162, 'O': 0x1164, # o=ㅐ, O=ㅒ
            'p': 0x1166, 'P': 0x1168, # p=ㅔ, P=ㅖ
            
            'a': 0x1106, 'A': 0x1106, # ㅁ
            's': 0x1102, 'S': 0x1102, # ㄴ
            'd': 0x110b, 'D': 0x110b, # ㅇ
            'f': 0x1105, 'F': 0x1105, # ㄹ
            'g': 0x1112, 'G': 0x1112, # ㅎ
            'h': 0x1169, 'H': 0x1169, # ㅗ
            'j': 0x1165, 'J': 0x1165, # ㅓ
            'k': 0x1161, 'K': 0x1161, # ㅏ
            'l': 0x1175, 'L': 0x1175, # ㅣ
            
            'z': 0x110f, 'Z': 0x110f, # ㅋ
            'x': 0x1110, 'X': 0x1110, # ㅌ
            'c': 0x110e, 'C': 0x110e, # ㅊ
            'v': 0x1111, 'V': 0x1111, # ㅍ
            'b': 0x1172, 'B': 0x1172, # ㅠ
            'n': 0x116e, 'N': 0x116e, # ㅜ
            'm': 0x1173, 'M': 0x1173, # ㅡ
        }
        return mapping.get(c, 0)

    def current_syllable(self):
        if self._cho == 0 and self._jung == 0 and self._jong == 0:
            return 0
        
        # Independent Jamo
        if self._cho and not self._jung and not self._jong:
            return self.compatibility_jamo(self._cho)
        if not self._cho and self._jung and not self._jong:
            return self.compatibility_jamo(self._jung)
        
        cho_idx = self.cho_index(self._cho) if self._cho else -1
        jung_idx = self.jung_index(self._jung) if self._jung else -1
        jong_idx = self.jong_index(self._jong) if self._jong else 0
        
        if cho_idx != -1 and jung_idx != -1:
            return 0xAC00 + (cho_idx * 21 * 28) + (jung_idx * 28) + jong_idx
        
        # Jung Only (Moa-jjiki transitional)
        if jung_idx != -1 and cho_idx == -1:
            return self.compatibility_jamo(self._jung)
            
        return 0

    def compatibility_jamo(self, u):
        if 0x1100 <= u <= 0x1112:
            base = u - 0x1100
            # ㄱ ㄲ ㄴ ㄷ ㄸ ㄹ ㅁ ㅂ ㅃ ㅅ ㅆ ㅇ ㅈ ㅉ ㅊ ㅋ ㅌ ㅍ ㅎ
            # 3131 3132 3134 3137 3138 3139 3141 3142 3143 3145 3146 3147 3148 3149 314A 314B 314C 314D 314E
            map_arr = [
                0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143,
                0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E
            ]
            if base < len(map_arr): return map_arr[base]
            
        if 0x1161 <= u <= 0x1175:
            base = u - 0x1161
            # ㅏ ㅐ ㅑ ㅒ ㅓ ㅔ ㅕ ㅖ ㅗ ㅘ ㅙ ㅚ ㅛ ㅜ ㅝ ㅞ ㅟ ㅠ ㅡ ㅢ ㅣ
            # 314F 3150 3151 3152 3153 3154 3155 3156 3157 3158 3159 315A 315B 315C 315D 315E 315F 3160 3161 3162 3163
            map_arr = [
                0x314F, 0x3150, 0x3151, 0x3152, 0x3153, 0x3154, 0x3155, 0x3156, 0x3157,
                0x3158, 0x3159, 0x315A, 0x315B, 0x315C, 0x315D, 0x315E, 0x315F, 0x3160,
                0x3161, 0x3162, 0x3163
            ]
            if base < len(map_arr): return map_arr[base]
            
        return u

    def cho_index(self, c):
        if 0x1100 <= c <= 0x1112: return c - 0x1100
        return -1

    def jung_index(self, c):
        if 0x1161 <= c <= 0x1175: return c - 0x1161
        return -1

    def jong_index(self, c):
        if 0x11A8 <= c <= 0x11C2: return c - 0x11A8 + 1
        return 0

    def backspace(self):
        if self._cho == 0 and self._jung == 0 and self._jong == 0:
            return False
        
        if self._jong != 0:
            j1, j2 = self.split_jong(self._jong)
            if j2 != 0:
                self._jong = j1
            else:
                self._jong = 0
            return True
            
        if self._jung != 0:
            j1, j2 = self.split_jung(self._jung)
            if j2 != 0:
                self._jung = j1
            else:
                self._jung = 0
            return True
            
        if self._cho != 0:
            self._cho = 0
            return True
            
        return False

    def process_code(self, key_char):
        # key_char should be a character 'a', 'b', etc.
        hangul = self.map_from_char(key_char)
        
        if hangul == 0:
            # Commit current if exists
            if self._cho or self._jung or self._jong:
                self._completed.append(chr(self.current_syllable()))
                self._cho = 0; self._jung = 0; self._jong = 0
            return False
            
        if self.is_cho(hangul):
            if self._jung == 0:
                if self._cho == 0:
                    self._cho = hangul
                else:
                    self._completed.append(chr(self.compatibility_jamo(self._cho)))
                    self._cho = hangul
            else:
                if self._jong == 0:
                    if self._cho == 0:
                        if self.moa_jjiki_enabled:
                            self._cho = hangul
                            return True
                        else:
                            self._completed.append(chr(self.current_syllable()))
                            self._cho = hangul; self._jung = 0; self._jong = 0
                            return True
                    
                    as_jong = self.cho_to_jong(hangul)
                    if as_jong:
                        self._jong = as_jong
                    else:
                        self._completed.append(chr(self.current_syllable()))
                        self._cho = hangul; self._jung = 0; self._jong = 0
                else:
                    # Cho+Jung+Jong
                    compound = self.combine_jong(self._jong, self.cho_to_jong(hangul))
                    if compound:
                        self._jong = compound
                    else:
                        self._completed.append(chr(self.current_syllable()))
                        self._cho = hangul; self._jung = 0; self._jong = 0
                        
        elif self.is_jung(hangul):
            if self._jong:
                j1, j2 = self.split_jong(self._jong)
                if j2:
                    self._jong = j1
                    next_cho = self.jong_to_cho(j2)
                    self._completed.append(chr(self.current_syllable()))
                    self._cho = next_cho; self._jung = hangul; self._jong = 0
                else:
                    next_cho = self.jong_to_cho(self._jong)
                    self._jong = 0
                    self._completed.append(chr(self.current_syllable()))
                    self._cho = next_cho; self._jung = hangul; self._jong = 0
                    
            elif self._jung:
                compound = self.combine_jung(self._jung, hangul)
                if compound:
                    self._jung = compound
                else:
                    self._completed.append(chr(self.current_syllable()))
                    self._cho = 0; self._jung = hangul; self._jong = 0
            else:
                self._jung = hangul # Independent or with Cho
                
        return True

    def cho_to_jong(self, c):
        mapping = {
            0x1100: 0x11A8, 0x1101: 0x11A9, 0x1102: 0x11AB, 0x1103: 0x11AE,
            0x1105: 0x11AF, 0x1106: 0x11B7, 0x1107: 0x11B8, 0x1109: 0x11BA,
            0x110A: 0x11BB, 0x110B: 0x11BC, 0x110C: 0x11BD, 0x110E: 0x11BE,
            0x110F: 0x11BF, 0x1110: 0x11C0, 0x1111: 0x11C1, 0x1112: 0x11C2
        }
        return mapping.get(c, 0)

    def jong_to_cho(self, c):
        mapping = {
            0x11A8: 0x1100, 0x11A9: 0x1101, 0x11AB: 0x1102, 0x11AE: 0x1103,
            0x11AF: 0x1105, 0x11B7: 0x1106, 0x11B8: 0x1107, 0x11BA: 0x1109,
            0x11BB: 0x110A, 0x11BC: 0x110B, 0x11BD: 0x110C, 0x11BE: 0x110E,
            0x11BF: 0x110F, 0x11C0: 0x1110, 0x11C1: 0x1111, 0x11C2: 0x1112
        }
        return mapping.get(c, 0)
        
    def combine_jung(self, a, b):
        if a == 0x1169 and b == 0x1161: return 0x116A # ㅘ
        if a == 0x1169 and b == 0x1162: return 0x116B # ㅙ
        if a == 0x1169 and b == 0x1175: return 0x116C # ㅚ
        if a == 0x116e and b == 0x1165: return 0x116F # ㅝ
        if a == 0x116e and b == 0x1166: return 0x1170 # ㅞ
        if a == 0x116e and b == 0x1175: return 0x1171 # ㅟ
        if a == 0x1173 and b == 0x1175: return 0x1174 # ㅢ
        return 0

    def split_jung(self, c):
        mapping = {
            0x116A: (0x1169, 0x1161), 0x116B: (0x1169, 0x1162), 0x116C: (0x1169, 0x1175),
            0x116F: (0x116e, 0x1165), 0x1170: (0x116e, 0x1166), 0x1171: (0x116e, 0x1175),
            0x1174: (0x1173, 0x1175)
        }
        return mapping.get(c, (c, 0))

    def combine_jong(self, a, b):
        if a == 0x11A8 and b == 0x11BA: return 0x11AA # ㄳ
        if a == 0x11AB and b == 0x11BD: return 0x11AC # ㄵ
        if a == 0x11AB and b == 0x11C2: return 0x11AD # ㄶ
        if a == 0x11AF and b == 0x11A8: return 0x11B0 # ㄺ
        if a == 0x11AF and b == 0x11B7: return 0x11B1 # ㄻ
        if a == 0x11AF and b == 0x11B8: return 0x11B2 # ㄼ
        if a == 0x11AF and b == 0x11BA: return 0x11B3 # ㄽ
        if a == 0x11AF and b == 0x11C0: return 0x11B4 # ㄾ
        if a == 0x11AF and b == 0x11C1: return 0x11B5 # ㄿ
        if a == 0x11AF and b == 0x11C2: return 0x11B6 # ㅀ
        if a == 0x11B8 and b == 0x11BA: return 0x11B9 # ㅄ
        return 0

    def split_jong(self, c):
        mapping = {
            0x11AA: (0x11A8, 0x11BA), 0x11AC: (0x11AB, 0x11BD), 0x11AD: (0x11AB, 0x11C2),
            0x11B0: (0x11AF, 0x11A8), 0x11B1: (0x11AF, 0x11B7), 0x11B2: (0x11AF, 0x11B8),
            0x11B3: (0x11AF, 0x11BA), 0x11B4: (0x11AF, 0x11C0), 0x11B5: (0x11AF, 0x11C1),
            0x11B6: (0x11AF, 0x11C2), 0x11B9: (0x11B8, 0x11BA)
        }
        return mapping.get(c, (c, 0))
