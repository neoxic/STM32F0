MEMORY {
	rom (rx)  : ORIGIN = 0x08000000, LENGTH = 16K
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 4K
}

INCLUDE cortex-m-generic.ld
