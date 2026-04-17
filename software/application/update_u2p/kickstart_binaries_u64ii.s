.section ".rodata"
.align 4 # which either means 4 or 2**4 depending on arch!

.global _ultimate_app_start
.type _ultimate_app_start, @object
_ultimate_app_start:
.incbin "ultimate.app"
.global _ultimate_app_end
_ultimate_app_end:
