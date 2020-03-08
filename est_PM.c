/*-
 * Copyright 2004 Colin Percival.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Colin Percival.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Has been added Pentium M 753G, 753H, 753I, 753J, 753K, 753L
 * Oleg Pyzin 2017
 */

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#if __FreeBSD_version >= 502000
#include <sys/smp.h>
#endif

#if __FreeBSD_version < 500000
#include <sys/time.h>
#else
#include <sys/timetc.h>
#endif

#if __FreeBSD_version < 500023
#include <sys/proc.h>
#else
#include <sys/pcpu.h>
#endif

#include <machine/md_var.h>

/* Names and numbers from IA-32 System Programming Guide */
#define MSR_PERF_STATUS		0x198
#define MSR_PERF_CTL		0x199

/* Specifies a frequency, and how to get it. */
typedef struct {
	uint16_t MHz;
	uint16_t ID;
} freq_info;

/* Identifying characteristics of a processor and its frequencies */
typedef struct {
	char * vendor;
	uint32_t ID;
	uint32_t BUSCLK;
	freq_info * freqtab;
} cpu_info;

#define ID16(MHz, mv, BUSCLK)				\
    ( ((MHz / BUSCLK) << 8) + ((mv ? mv - 700 : 0) >> 4) )
#define ID32(MHz_hi, mv_hi, MHz_lo, mv_lo, BUSCLK)	\
    ( (ID16(MHz_lo, mv_lo, BUSCLK) << 16) + (ID16(MHz_hi, mv_hi, BUSCLK)) )
#define FREQ_INFO_100(MHz, mv)				\
    { MHz, ID16(MHz, mv, 100) }
#define INTEL_100(tab, zhi, vhi, zlo, vlo)		\
    { GenuineIntel, ID32(zhi, vhi, zlo, vlo, 100), 100, tab }

char GenuineIntel[12] = "GenuineIntel";

/*
 * Data from
 * Intel Pentium M Processor Datasheet (Order Number 252612), Table 5
 */
static freq_info PM17_130[] = {
	/* 130nm 1.70GHz Pentium M */
	FREQ_INFO_100(1700, 1484),
	FREQ_INFO_100(1400, 1308),
	FREQ_INFO_100(1200, 1228),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1004),
	FREQ_INFO_100( 600,  956),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM16_130[] = {
	/* 130nm 1.60GHz Pentium M */
	FREQ_INFO_100(1600, 1484),
	FREQ_INFO_100(1400, 1420),
	FREQ_INFO_100(1200, 1276),
	FREQ_INFO_100(1000, 1164),
	FREQ_INFO_100( 800, 1036),
	FREQ_INFO_100( 600,  956),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM15_130[] = {
	/* 130nm 1.50GHz Pentium M */
	FREQ_INFO_100(1500, 1484),
	FREQ_INFO_100(1400, 1452),
	FREQ_INFO_100(1200, 1356),
	FREQ_INFO_100(1000, 1228),
	FREQ_INFO_100( 800, 1116),
	FREQ_INFO_100( 600,  956),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM14_130[] = {
	/* 130nm 1.40GHz Pentium M */
	FREQ_INFO_100(1400, 1484),
	FREQ_INFO_100(1200, 1436),
	FREQ_INFO_100(1000, 1308),
	FREQ_INFO_100( 800, 1180),
	FREQ_INFO_100( 600,  956),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM13_130[] = {
	/* 130nm 1.30GHz Pentium M */
	FREQ_INFO_100(1300, 1388),
	FREQ_INFO_100(1200, 1356),
	FREQ_INFO_100(1000, 1292),
	FREQ_INFO_100( 800, 1260),
	FREQ_INFO_100( 600,  956),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM13_LV_130[] = {
	/* 130nm 1.30GHz Low Voltage Pentium M */
	FREQ_INFO_100(1300, 1180),
	FREQ_INFO_100(1200, 1164),
	FREQ_INFO_100(1100, 1100),
	FREQ_INFO_100(1000, 1020),
	FREQ_INFO_100( 900, 1004),
	FREQ_INFO_100( 800,  988),
	FREQ_INFO_100( 600,  956),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM12_LV_130[] = {
	/* 130 nm 1.20GHz Low Voltage Pentium M */
	FREQ_INFO_100(1200, 1180),
	FREQ_INFO_100(1100, 1164),
	FREQ_INFO_100(1000, 1100),
	FREQ_INFO_100( 900, 1020),
	FREQ_INFO_100( 800, 1004),
	FREQ_INFO_100( 600,  956),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM11_LV_130[] = {
	/* 130 nm 1.10GHz Low Voltage Pentium M */
	FREQ_INFO_100(1100, 1180),
	FREQ_INFO_100(1000, 1164),
	FREQ_INFO_100( 900, 1100),
	FREQ_INFO_100( 800, 1020),
	FREQ_INFO_100( 600,  956),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM11_ULV_130[] = {
	/* 130 nm 1.10GHz Ultra Low Voltage Pentium M */
	FREQ_INFO_100(1100, 1004),
	FREQ_INFO_100(1000,  988),
	FREQ_INFO_100( 900,  972),
	FREQ_INFO_100( 800,  956),
	FREQ_INFO_100( 600,  844),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM10_ULV_130[] = {
	/* 130 nm 1.00GHz Ultra Low Voltage Pentium M */
	FREQ_INFO_100(1000, 1004),
	FREQ_INFO_100( 900,  988),
	FREQ_INFO_100( 800,  972),
	FREQ_INFO_100( 600,  844),
	FREQ_INFO_100(   0,    0),
};

/*
 * Data from
 * Intel Pentium M Processor on 90nm Process with 2-MB L2 Cache
 * Datasheet (Order Number 302189), Table 5
 */
static freq_info PM_765A_90[] = {
	/* 90 nm 2.10GHz Pentium M, VID #A */
	FREQ_INFO_100(2100, 1340),
	FREQ_INFO_100(1800, 1276),
	FREQ_INFO_100(1600, 1228),
	FREQ_INFO_100(1400, 1180),
	FREQ_INFO_100(1200, 1132),
	FREQ_INFO_100(1000, 1084),
	FREQ_INFO_100( 800, 1036),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_765B_90[] = {
	/* 90 nm 2.10GHz Pentium M, VID #B */
	FREQ_INFO_100(2100, 1324),
	FREQ_INFO_100(1800, 1260),
	FREQ_INFO_100(1600, 1212),
	FREQ_INFO_100(1400, 1180),
	FREQ_INFO_100(1200, 1132),
	FREQ_INFO_100(1000, 1084),
	FREQ_INFO_100( 800, 1036),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_765C_90[] = {
	/* 90 nm 2.10GHz Pentium M, VID #C */
	FREQ_INFO_100(2100, 1308),
	FREQ_INFO_100(1800, 1244),
	FREQ_INFO_100(1600, 1212),
	FREQ_INFO_100(1400, 1164),
	FREQ_INFO_100(1200, 1116),
	FREQ_INFO_100(1000, 1084),
	FREQ_INFO_100( 800, 1036),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_765E_90[] = {
	/* 90 nm 2.10GHz Pentium M, VID #E */
	FREQ_INFO_100(2100, 1356),
	FREQ_INFO_100(1800, 1292),
	FREQ_INFO_100(1600, 1244),
	FREQ_INFO_100(1400, 1196),
	FREQ_INFO_100(1200, 1148),
	FREQ_INFO_100(1000, 1100),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_755A_90[] = {
	/* 90 nm 2.00GHz Pentium M, VID #A */
	FREQ_INFO_100(2000, 1340),
	FREQ_INFO_100(1800, 1292),
	FREQ_INFO_100(1600, 1244),
	FREQ_INFO_100(1400, 1196),
	FREQ_INFO_100(1200, 1148),
	FREQ_INFO_100(1000, 1100),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_755B_90[] = {
	/* 90 nm 2.00GHz Pentium M, VID #B */
	FREQ_INFO_100(2000, 1324),
	FREQ_INFO_100(1800, 1276),
	FREQ_INFO_100(1600, 1228),
	FREQ_INFO_100(1400, 1180),
	FREQ_INFO_100(1200, 1132),
	FREQ_INFO_100(1000, 1084),
	FREQ_INFO_100( 800, 1036),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_755C_90[] = {
	/* 90 nm 2.00GHz Pentium M, VID #C */
	FREQ_INFO_100(2000, 1308),
	FREQ_INFO_100(1800, 1276),
	FREQ_INFO_100(1600, 1228),
	FREQ_INFO_100(1400, 1180),
	FREQ_INFO_100(1200, 1132),
	FREQ_INFO_100(1000, 1084),
	FREQ_INFO_100( 800, 1036),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_755D_90[] = {
	/* 90 nm 2.00GHz Pentium M, VID #D */
	FREQ_INFO_100(2000, 1276),
	FREQ_INFO_100(1800, 1244),
	FREQ_INFO_100(1600, 1196),
	FREQ_INFO_100(1400, 1164),
	FREQ_INFO_100(1200, 1116),
	FREQ_INFO_100(1000, 1084),
	FREQ_INFO_100( 800, 1036),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_745A_90[] = {
	/* 90 nm 1.80GHz Pentium M, VID #A */
	FREQ_INFO_100(1800, 1340),
	FREQ_INFO_100(1600, 1292),
	FREQ_INFO_100(1400, 1228),
	FREQ_INFO_100(1200, 1164),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_745B_90[] = {
	/* 90 nm 1.80GHz Pentium M, VID #B */
	FREQ_INFO_100(1800, 1324),
	FREQ_INFO_100(1600, 1276),
	FREQ_INFO_100(1400, 1212),
	FREQ_INFO_100(1200, 1164),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_745C_90[] = {
	/* 90 nm 1.80GHz Pentium M, VID #C */
	FREQ_INFO_100(1800, 1308),
	FREQ_INFO_100(1600, 1260),
	FREQ_INFO_100(1400, 1212),
	FREQ_INFO_100(1200, 1148),
	FREQ_INFO_100(1000, 1100),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_745D_90[] = {
	/* 90 nm 1.80GHz Pentium M, VID #D */
	FREQ_INFO_100(1800, 1276),
	FREQ_INFO_100(1600, 1228),
	FREQ_INFO_100(1400, 1180),
	FREQ_INFO_100(1200, 1132),
	FREQ_INFO_100(1000, 1084),
	FREQ_INFO_100( 800, 1036),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_735A_90[] = {
	/* 90 nm 1.70GHz Pentium M, VID #A */
	FREQ_INFO_100(1700, 1340),
	FREQ_INFO_100(1400, 1244),
	FREQ_INFO_100(1200, 1180),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_735B_90[] = {
	/* 90 nm 1.70GHz Pentium M, VID #B */
	FREQ_INFO_100(1700, 1324),
	FREQ_INFO_100(1400, 1244),
	FREQ_INFO_100(1200, 1180),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_735C_90[] = {
	/* 90 nm 1.70GHz Pentium M, VID #C */
	FREQ_INFO_100(1700, 1308),
	FREQ_INFO_100(1400, 1228),
	FREQ_INFO_100(1200, 1164),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_735D_90[] = {
	/* 90 nm 1.70GHz Pentium M, VID #D */
	FREQ_INFO_100(1700, 1276),
	FREQ_INFO_100(1400, 1212),
	FREQ_INFO_100(1200, 1148),
	FREQ_INFO_100(1000, 1100),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_725A_90[] = {
	/* 90 nm 1.60GHz Pentium M, VID #A */
	FREQ_INFO_100(1600, 1340),
	FREQ_INFO_100(1400, 1276),
	FREQ_INFO_100(1200, 1212),
	FREQ_INFO_100(1000, 1132),
	FREQ_INFO_100( 800, 1068),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_725B_90[] = {
	/* 90 nm 1.60GHz Pentium M, VID #B */
	FREQ_INFO_100(1600, 1324),
	FREQ_INFO_100(1400, 1260),
	FREQ_INFO_100(1200, 1196),
	FREQ_INFO_100(1000, 1132),
	FREQ_INFO_100( 800, 1068),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_725C_90[] = {
	/* 90 nm 1.60GHz Pentium M, VID #C */
	FREQ_INFO_100(1600, 1308),
	FREQ_INFO_100(1400, 1244),
	FREQ_INFO_100(1200, 1180),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_725D_90[] = {
	/* 90 nm 1.60GHz Pentium M, VID #D */
	FREQ_INFO_100(1600, 1276),
	FREQ_INFO_100(1400, 1228),
	FREQ_INFO_100(1200, 1164),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_715A_90[] = {
	/* 90 nm 1.50GHz Pentium M, VID #A */
	FREQ_INFO_100(1500, 1340),
	FREQ_INFO_100(1200, 1228),
	FREQ_INFO_100(1000, 1148),
	FREQ_INFO_100( 800, 1068),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_715B_90[] = {
	/* 90 nm 1.50GHz Pentium M, VID #B */
	FREQ_INFO_100(1500, 1324),
	FREQ_INFO_100(1200, 1212),
	FREQ_INFO_100(1000, 1148),
	FREQ_INFO_100( 800, 1068),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_715C_90[] = {
	/* 90 nm 1.50GHz Pentium M, VID #C */
	FREQ_INFO_100(1500, 1308),
	FREQ_INFO_100(1200, 1212),
	FREQ_INFO_100(1000, 1132),
	FREQ_INFO_100( 800, 1068),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_715D_90[] = {
	/* 90 nm 1.50GHz Pentium M, VID #D */
	FREQ_INFO_100(1500, 1276),
	FREQ_INFO_100(1200, 1180),
	FREQ_INFO_100(1000, 1116),
	FREQ_INFO_100( 800, 1052),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_738_90[] = {
	/* 90 nm 1.40GHz Low Voltage Pentium M */
	FREQ_INFO_100(1400, 1116),
	FREQ_INFO_100(1300, 1116),
	FREQ_INFO_100(1200, 1100),
	FREQ_INFO_100(1100, 1068),
	FREQ_INFO_100(1000, 1052),
	FREQ_INFO_100( 900, 1036),
	FREQ_INFO_100( 800, 1020),
	FREQ_INFO_100( 600,  988),
	FREQ_INFO_100(   0,    0),
};
/*===================================================*/
/* ---->                                             */
/* Data from Datasheet Intel Pentium M processor on
   90 nm Process with 2-MB L2 Cache. Document Number
   302189-008 Table 3-6.
*/
static freq_info PM_753G_90[] = {
        /* 90 nm 1.20Ghz Ultra Low Voltage Pentium M */
        FREQ_INFO_100(1200,  956),
        FREQ_INFO_100(1100,  940),
        FREQ_INFO_100(1000,  908),
        FREQ_INFO_100( 900,  892),
        FREQ_INFO_100( 800,  860),
        FREQ_INFO_100( 600,  812),
        FREQ_INFO_100(   0,    0),
};
static freq_info PM_753H_90[] = {
        /* 90 nm 1.20Ghz Ultra Low Voltage Pentium M */
        FREQ_INFO_100(1200,  940),
        FREQ_INFO_100(1100,  924),
        FREQ_INFO_100(1000,  908),
        FREQ_INFO_100( 900,  876),
        FREQ_INFO_100( 800,  860),
        FREQ_INFO_100( 600,  812),
        FREQ_INFO_100(   0,    0),
};
static freq_info PM_753I_90[] = {
        /* 90 nm 1.20Ghz Ultra Low Voltage Pentium M */
        FREQ_INFO_100(1200,  924),
        FREQ_INFO_100(1100,  908),
        FREQ_INFO_100(1000,  892),
        FREQ_INFO_100( 900,  876),
        FREQ_INFO_100( 800,  860),
        FREQ_INFO_100( 600,  812),
        FREQ_INFO_100(   0,    0),
};
static freq_info PM_753J_90[] = {
        /* 90 nm 1.10Ghz Ultra Low Voltage Pentium M */
        FREQ_INFO_100(1200,  908),
        FREQ_INFO_100(1100,  892),
        FREQ_INFO_100(1000,  876),
        FREQ_INFO_100( 900,  860),
        FREQ_INFO_100( 800,  844),
        FREQ_INFO_100( 600,  812),
        FREQ_INFO_100(   0,    0),
};
static freq_info PM_753K_90[] = {
        /* 90 nm 1.10Ghz Ultra Low Voltage Pentium M */
        FREQ_INFO_100(1200,  892),
        FREQ_INFO_100(1100,  892),
        FREQ_INFO_100(1000,  876),
        FREQ_INFO_100( 900,  860),
        FREQ_INFO_100( 800,  844),
        FREQ_INFO_100( 600,  812),
        FREQ_INFO_100(   0,    0),
};
static freq_info PM_753L_90[] = {
        /* 90 nm 1.10Ghz Ultra Low Voltage Pentium M */
        FREQ_INFO_100(1200,  876),
        FREQ_INFO_100(1100,  876),
        FREQ_INFO_100(1000,  860),
        FREQ_INFO_100( 900,  844),
        FREQ_INFO_100( 800,  844),
        FREQ_INFO_100( 600,  812),
        FREQ_INFO_100(   0,    0),
};
/*  <---------- Added                                */
/*===================================================*/
static freq_info PM_733_90[] = {
	/* 90 nm 1.10GHz Ultra Low Voltage Pentium M */
	FREQ_INFO_100(1100,  940),
	FREQ_INFO_100(1000,  924),
	FREQ_INFO_100( 900,  892),
	FREQ_INFO_100( 800,  876),
	FREQ_INFO_100( 600,  812),
	FREQ_INFO_100(   0,    0),
};
static freq_info PM_723_90[] = {
	/* 90 nm 1.00GHz Ultra Low Voltage Pentium M */
	FREQ_INFO_100(1000,  940),
	FREQ_INFO_100( 900,  908),
	FREQ_INFO_100( 800,  876),
	FREQ_INFO_100( 600,  812),
	FREQ_INFO_100(   0,    0),
};

/*
 * XXX When adding new processors here, check that the est_frequencies
 * buffer (declared below) is still large enough.
 */
static cpu_info ESTprocs[] = {
	INTEL_100(PM17_130,	1700, 1484, 600, 956),
	INTEL_100(PM16_130,	1600, 1484, 600, 956),
	INTEL_100(PM15_130,	1500, 1484, 600, 956),
	INTEL_100(PM14_130,	1400, 1484, 600, 956),
	INTEL_100(PM13_130,	1300, 1388, 600, 956),
	INTEL_100(PM13_LV_130,	1300, 1180, 600, 956),
	INTEL_100(PM12_LV_130,	1200, 1180, 600, 956),
	INTEL_100(PM11_LV_130,	1100, 1180, 600, 956),
	INTEL_100(PM11_ULV_130,	1100, 1004, 600, 844),
	INTEL_100(PM10_ULV_130,	1000, 1004, 600, 844),
	INTEL_100(PM_765A_90,	2100, 1340, 600, 988),
	INTEL_100(PM_765B_90,	2100, 1324, 600, 988),
	INTEL_100(PM_765C_90,	2100, 1308, 600, 988),
	INTEL_100(PM_765E_90,	2100, 1356, 600, 988),
	INTEL_100(PM_755A_90,	2000, 1340, 600, 988),
	INTEL_100(PM_755B_90,	2000, 1324, 600, 988),
	INTEL_100(PM_755C_90,	2000, 1308, 600, 988),
	INTEL_100(PM_755D_90,	2000, 1276, 600, 988),
	INTEL_100(PM_745A_90,	1800, 1340, 600, 988),
	INTEL_100(PM_745B_90,	1800, 1324, 600, 988),
	INTEL_100(PM_745C_90,	1800, 1308, 600, 988),
	INTEL_100(PM_745D_90,	1800, 1276, 600, 988),
	INTEL_100(PM_735A_90,	1700, 1340, 600, 988),
	INTEL_100(PM_735B_90,	1700, 1324, 600, 988),
	INTEL_100(PM_735C_90,	1700, 1308, 600, 988),
	INTEL_100(PM_735D_90,	1700, 1276, 600, 988),
	INTEL_100(PM_725A_90,	1600, 1340, 600, 988),
	INTEL_100(PM_725B_90,	1600, 1324, 600, 988),
	INTEL_100(PM_725C_90,	1600, 1308, 600, 988),
	INTEL_100(PM_725D_90,	1600, 1276, 600, 988),
	INTEL_100(PM_715A_90,	1500, 1340, 600, 988),
	INTEL_100(PM_715B_90,	1500, 1324, 600, 988),
	INTEL_100(PM_715C_90,	1500, 1308, 600, 988),
	INTEL_100(PM_715D_90,	1500, 1276, 600, 988),
	INTEL_100(PM_738_90,	1400, 1116, 600, 988),
/*============================================================*/
/* ---->                                             */
/* Data from Datasheet Intel Pentium M processor on
   90 nm Process with 2-MB L2 Cache. Document Number
   302189-008 Table 3-6.
*/
	INTEL_100(PM_753G_90,   1200,  956, 600, 812),
	INTEL_100(PM_753H_90,   1200,  940, 600, 812),
	INTEL_100(PM_753I_90,   1200,  924, 600, 812),
	INTEL_100(PM_753J_90,   1200,  908, 600, 812),
	INTEL_100(PM_753K_90,   1200,  892, 600, 812),
	INTEL_100(PM_753L_90,   1200,  876, 600, 812),
/*============================================================*/
	INTEL_100(PM_733_90,	1100,  940, 600, 812),
	INTEL_100(PM_723_90,	1000,  940, 600, 812),
	{ NULL, 0, 0, NULL },
};

static int est_verbose = 0;
SYSCTL_INT(_hw, OID_AUTO, est_verbose, CTLFLAG_RW, &est_verbose,
	   0, "Log CPU frequency changes");

static freq_info * freq_list = NULL;		/* NULL if EST is disabled */

static int
est_sysctl_mhz(SYSCTL_HANDLER_ARGS)
{
	uint64_t msr;
	freq_info * f;
	int MHz, MHz_wanted;
	int err = 0;

	if (freq_list == NULL)
		return (EOPNOTSUPP);

	/* Read current status, and make sure it's on our table */
	msr = rdmsr(MSR_PERF_STATUS) & 0xffff;
	for (f = freq_list; f->MHz != 0; f++)
		if (f->ID == msr)
			break;
	if (f->MHz == 0) {
		printf("MSR_PERF_STATUS reports clock ratio (%d) "
		    "not in freq_list.  Disabling EST.\n",
		    (int)(msr >> 16));
		freq_list = NULL;
		return (EINVAL);
	}
	MHz = f->MHz;

	if (req->newptr) {
		/*
		 * Check that the TSC isn't being used as a timecounter.
		 * If it is, then return EBUSY and refuse to change the
		 * clock speed.
		 */
		if (strcmp(timecounter->tc_name, "TSC") == 0)
			return EBUSY;

		err = SYSCTL_IN(req, &MHz_wanted, sizeof(int));
		if (err)
			return err;

		/* Look up desired frequency in table */
		for (f = freq_list; f->MHz != 0; f++)
			if (f->MHz == MHz_wanted)
				break;
		if (f->MHz == 0)
			return (EOPNOTSUPP);

		if (est_verbose)
			printf("Changing CPU frequency from %d MHz "
			    "to %d MHz.\n", MHz, MHz_wanted);

		msr = rdmsr(MSR_PERF_CTL);
		msr = (msr & ~(uint64_t)(0xffff)) | f->ID;
		wrmsr(MSR_PERF_CTL, msr);

		/*
		 * Sleep for a short time, to let the cpu find
		 * its new frequency before we return to the user.
		 */
		tsleep(&freq_list, PUSER, "EST", 1);
	} else {
		err = SYSCTL_OUT(req, &MHz, sizeof(int));
	}

	return err;
}

SYSCTL_PROC(_hw, OID_AUTO, est_curfreq, CTLTYPE_INT | CTLFLAG_RW, 0, 0,
	    &est_sysctl_mhz, "I", "Current CPU frequency for Enhanced SpeedStep");

/*
 * XXX fixed buffer size XXX
 * This buffer is large enough for up to 16 frequencies of < 10GHz.
 * If we need more than that, increase the buffer size.  Since this
 * buffer is filled using data declared above, this is safe.
 */
static char est_frequencies[80] = "";
SYSCTL_STRING(_hw, OID_AUTO, est_freqs, CTLFLAG_RD, est_frequencies, 0,
	"CPU frequencies supported by Enhanced SpeedStep");

static int
findcpu(char * vendor, uint64_t msr, uint32_t BUSCLK)
{
	cpu_info * p;
	freq_info * f;
	char freq[6];
	uint32_t ID;
	uint16_t ID16;

	ID = msr >> 32;
	ID16 = msr & 0xffff;

	/* Find a table which matches (vendor, ID, BUSCLK) */
	for (p = ESTprocs; p->ID != 0; p++)
		if ((bcmp(p->vendor, vendor, 12) == 0) &&
		    (p->ID == ID) &&
		    (p->BUSCLK == BUSCLK))
			break;
	if (p->ID == 0)
		return (EOPNOTSUPP);

	/* Make sure the current setpoint is on the table */
	for (f = p->freqtab; f->ID != 0; f++)
		if (f->ID == ID16)
			break;
	if (f->ID == 0)
		return (EOPNOTSUPP);

	/* Print status message and enable EST */
	printf("Enhanced Speedstep running at %d MHz.\n", f->MHz);
	freq_list = p->freqtab;

	/*
	 * Generate est_frequencies string, which lists the frequencies
	 * supported in increasing order.  Our tables are in the opposite
	 * order (duh!) so read the table backwards.
	 */
	est_frequencies[0] = 0;
	for (f = freq_list; f->ID != 0; f++);
	for (f--;; f--) {
		if (est_frequencies[0] == 0)
			sprintf(freq, "%d", f->MHz);
		else
			sprintf(freq, " %d", f->MHz);
		strcat(est_frequencies, freq);
		if (f == freq_list)
			break;
	}

	return 0;
}

static int
est_loader(struct module *m, int what, void *arg)
{
	uint64_t msr;
	u_int p[4];
	int ncpu, ncpulen;
	int err = 0;

	switch (what) {
	case MOD_LOAD:
		/*
		 * Work out how many cpus we have.  We don't support
		 * Enhanced Speedstep on SMP systems.  Fortunately, no
		 * such systems exist yet.  (When they exist, I'll write
		 * the code for them, m'kay?)
		 */
		ncpulen = sizeof(ncpu);
#if __FreeBSD_version >= 502000
		ncpu = mp_ncpus;
#elif __FreeBSD_version >= 500023
		err = kernel_sysctlbyname(curthread, "hw.ncpu", &ncpu,
		    &ncpulen, NULL, 0, NULL);
#else
		err = kernel_sysctlbyname(curproc, "hw.ncpu", &ncpu,
		    &ncpulen, NULL, 0, NULL);
#endif
		if (err)
			break;
		if (ncpu != 1) {
			printf("Enhanced SpeedStep not supported "
			    "with more than one processor.\n");
			break;
		}

		/* Check that CPUID is supported */
		if (cpu_high == 0) {
			printf("Enhanced Speedstep not supported "
			    "on this processor.\n");
			break;
		}

		/*
		 * Enhanced Speedstep isn't supported by any processors
		 * from vendors other than Intel.
		 */
		if (strncmp(cpu_vendor, GenuineIntel, 12)) {
			printf("Enhanced Speedstep not supported "
			    "on this processor.\n");
			break;
		}

		/* Read capability bits */
		do_cpuid(1, p);

		/* Does the processor claim to support Enhanced Speedstep? */
		if ((p[2] & 0x80) == 0) {
			printf("Enhanced Speedstep not supported "
			    "on this processor.\n");
			break;
		}

		/* Identify the exact CPU model */
		msr = rdmsr(MSR_PERF_STATUS);
		if (findcpu(cpu_vendor, msr, 100) != 0) {
			printf("Processor claims to support "
			    "Enhanced Speedstep, but is not recognized.\n"
			    "Please update driver or contact "
			    "the maintainer.\n"
			    "cpu_vendor = %12s msr = %0llx, BUSCLK = %x.\n",
			    cpu_vendor, (unsigned long long)msr, 100);
			break;
		}

		break;
	case MOD_UNLOAD:
		break;
	default:
		err = EINVAL;
		break;
	}

	return err;
}

static moduledata_t est_mod = {
	"est_PM",
	est_loader,
	NULL
};

DECLARE_MODULE(est, est_mod, SI_SUB_KLD, SI_ORDER_ANY);
