#pragma once

#define MMIO32(x)   (*(volatile unsigned long *)(x))
#define MMIO16(x)   (*(volatile unsigned short *)(x))
#define MMIO8(x)    (*(volatile unsigned char *)(x))