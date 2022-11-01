.section __DATA, __const
.global _g_fontawesome_data, _g_fontawesome_size

_g_fontawesome_data:
.incbin "fontawesome-webfont.ttf"
_g_fontawesome_size:
.int _g_fontawesome_size - _g_fontawesome_data

