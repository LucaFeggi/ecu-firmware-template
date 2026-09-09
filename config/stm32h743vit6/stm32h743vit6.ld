ENTRY(Reset_Handler)

MEMORY
{
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 1920K
  DTCM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  RAM_D1 (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
  RAM_D2 (xrw) : ORIGIN = 0x30000000, LENGTH = 288K
  RAM_D3 (xrw) : ORIGIN = 0x38000000, LENGTH = 64K
}

_estack = ORIGIN(DTCM) + LENGTH(DTCM);

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text .text.*)
    *(.rodata .rodata.*)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > FLASH

  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > FLASH

  .data :
  {
    . = ALIGN(16384);
    _sdata = .;
    _seth_dma = .;
    KEEP(*(.eth_dma))
    . = ALIGN(32);
    _eeth_dma_payload = .;
    . = _seth_dma + 0x4000;
    *(.data .data.*)
    . = ALIGN(4);
    _edata = .;
  } > RAM_D2 AT > FLASH
  _sidata = LOADADDR(.data);

  .bss (NOLOAD) :
  {
    . = ALIGN(8);
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(8);
    _ebss = .;
  } > RAM_D1

  .noinit (NOLOAD) :
  {
    . = ALIGN(8);
    *(.noinit .noinit.*)
    . = ALIGN(8);
  } > RAM_D3

  .heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    _end = .;
    end = .;
  } > DTCM

  /DISCARD/ :
  {
    *(.note.GNU-stack)
    *(.gnu_debuglink)
    *(.gnu.lto_*)
  }
}

ASSERT(_eeth_dma_payload - _seth_dma <= 0x4000,
       "Ethernet DMA storage exceeds its 16 KiB MPU region")
ASSERT(_edata <= ORIGIN(RAM_D2) + LENGTH(RAM_D2), "RAM_D2 overflow")
ASSERT(_ebss <= ORIGIN(RAM_D1) + LENGTH(RAM_D1), "RAM_D1 overflow")
