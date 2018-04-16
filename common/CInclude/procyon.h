#ifndef PROCYON_H__
#define PROCYON_H__

#define MMIO32(x)   (*(volatile unsigned long *)(x))
#define MMIO16(x)   (*(volatile unsigned short *)(x))
#define MMIO8(x)    (*(volatile unsigned char *)(x))

/* System variable locations */
#define P_MEMTOP 0x100000
#define P_MEMBOT 0x100004

#define P_ROMSTART 0x000000
#define P_ROMEND   0x0FFFFF

#define P_RAMSTART 0x100000
#define P_RAMEND   0x1FFFFF

#define P_ROMDISK  0x80000

#endif
