/* ============================================================================
 * STM32G474RET6 应用链接脚本
 * 分区约定见 docs/flash-partition.md：
 *   0x08000000 Bootloader(32KB，预留) / 0x08008000 应用(448KB) / 0x08078000 参数(32KB)
 * 内存：SRAM1 96KB @0x20000000，SRAM2 32KB @0x20018000（电机控制 DMA 缓冲预留）
 * ==========================================================================*/

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM); /* 主栈顶（MSP），栈大小见 _Min_Stack_Size */
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x2000; /* 8KB：留给中断嵌套与控制环 ISR */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08008000, LENGTH = 448K
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 96K
  RAM2  (xrw) : ORIGIN = 0x20018000, LENGTH = 32K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    . = ALIGN(4);
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } >FLASH

  /* KEEP 防止 --gc-sections 裁掉 crtn.o 的 .init/.fini 结尾（否则 _init 不完整会崩） */
  .init :
  {
    KEEP (*(.init))
  } >FLASH

  .fini :
  {
    KEEP (*(.fini))
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  _etext = .;          /* 只读数据结束（.data 加载地址） */
  _sidata = _etext;    /* startup 文件使用的别名 */

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    __data_start__ = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
    __data_end__ = .;
  } >RAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = .;
  } >RAM

  /* SRAM2：电机控制高优先级缓冲（电流采样 DMA、CORDIC 数据）预留段 */
  .sram2 (NOLOAD) :
  {
    . = ALIGN(4);
    __sram2_start__ = .;
    *(.sram2)
    *(.sram2*)
    . = ALIGN(4);
    __sram2_end__ = .;
  } >RAM2

  /* 故障现场固定区（core/fault）：复位不清除，崩溃信息跨复位保留 */
  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    __noinit_start__ = .;
    *(.noinit)
    *(.noinit*)
    . = ALIGN(4);
    __noinit_end__ = .;
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }
}
