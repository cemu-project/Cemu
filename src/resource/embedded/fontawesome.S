.section .rodata,"",%progbits
.global g_fontawesome_data, g_fontawesome_size

g_fontawesome_data:
.incbin "fontawesome-webfont.ttf"
g_fontawesome_size:
.int g_fontawesome_size - g_fontawesome_data

.section .note.GNU-stack,"",%progbits