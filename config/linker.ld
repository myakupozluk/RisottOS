ENTRY(_start)

SECTIONS
{
    . = 0x1000;       /* kernel starts loading at this address */

    .text : { *(.text) }     /* code section */
    .data : { *(.data) }     /* initialized data */
    .bss  : { *(.bss)  }     /* uninitialized data */
}