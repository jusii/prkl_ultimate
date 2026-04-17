.section ".rodata"
.align 4
.global _license_text
.type _license_text, @object
_license_text:
.incbin "licenses.txt"
.byte 0
_license_text_end:

.global _license_text_size
.type _license_text_size, @object
_license_text_size:
    .long _license_text_end - _license_text
