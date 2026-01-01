#pragma once
#include "bit.h"

// Configuration Registers

#define PVR    287

#define HID0   1008
#define HID1   1009
#define HID2   920
#define HID4   1011

// Memory Management Registers

#define IBAT0U 528
#define IBAT0L 529
#define IBAT1U 530
#define IBAT1L 531
#define IBAT2U 532
#define IBAT2L 533
#define IBAT3U 534
#define IBAT3L 535
#define IBAT4U 560
#define IBAT4L 561
#define IBAT5U 562
#define IBAT5L 563
#define IBAT6U 564
#define IBAT6L 565
#define IBAT7U 566
#define IBAT7L 567

#define DBAT0U 536
#define DBAT0L 537
#define DBAT1U 538
#define DBAT1L 539
#define DBAT2U 540
#define DBAT2L 541
#define DBAT3U 542
#define DBAT3L 543
#define DBAT4U 568
#define DBAT4L 569
#define DBAT5U 570
#define DBAT5L 571
#define DBAT6U 572
#define DBAT6L 573
#define DBAT7U 574
#define DBAT7L 575

#define SDR1   25

// Exception Handling Registers

#define SPRG0  272
#define SPRG1  273
#define SPRG2  274
#define SPRG3  275

#define DAR    19
#define DSISR  18

#define SRR0   26
#define SRR1   27

// Miscellaneous Registers

#define EAR    282
#define DABR   1013
#define WPAR   921
#define TBL    284
#define TBU    285
#define L2CR   1017
#define DEC    22
#define IABR   1010
#define DMAL   923
#define DMAU   922

// Quantization Registers

#define GQR0   912
#define GQR1   913
#define GQR2   914
#define GQR3   915
#define GQR4   916
#define GQR5   917
#define GQR6   918
#define GQR7   919

// Performance Monitor Registers (For Reading)

#define UPMC1  937
#define UPMC2  938
#define UPMC3  941
#define UPMC4  942
#define UMMCR0 936
#define UMMCR1 940
#define USIA   939

// Performance Monitor Registers

#define PMC1   953
#define PMC2   954
#define PMC3   957
#define PMC4   958
#define MMCR0  952
#define MMCR1  956
#define SIA    955

// Power/Thermal Management Registers

#define THRM1  1020
#define THRM2  1021
#define THRM3  1022
#define TDCL   1012
#define TDCH   1018
#define ICTC   1019

//-----------------------------------------------------------------------------

#define MSR_POW     PPCBIT(13)
#define MSR_ILE     PPCBIT(15)
#define MSR_EE      PPCBIT(16)
#define MSR_PR      PPCBIT(17)
#define MSR_FP      PPCBIT(18)
#define MSR_ME      PPCBIT(19)
#define MSR_FE0     PPCBIT(20)
#define MSR_SE      PPCBIT(21)
#define MSR_BE      PPCBIT(22)
#define MSR_FE1     PPCBIT(23)
#define MSR_IP      PPCBIT(25)
#define MSR_IR      PPCBIT(26)
#define MSR_DR      PPCBIT(27)
#define MSR_PM      PPCBIT(29)
#define MSR_RI      PPCBIT(30)
#define MSR_LE      PPCBIT(31)

#define HID0_EMCP   PPCBIT(0)
#define HID0_DBP    PPCBIT(1)
#define HID0_EBA    PPCBIT(2)
#define HID0_EBD    PPCBIT(3)
#define HID0_BCLK   PPCBIT(4)
#define HID0_ECLK   PPCBIT(6)
#define HID0_PAR    PPCBIT(7)
#define HID0_DOZE   PPCBIT(8)
#define HID0_NAP    PPCBIT(9)
#define HID0_SLEEP  PPCBIT(10)
#define HID0_DPM    PPCBIT(11)
#define HID0_NHR    PPCBIT(15)
#define HID0_ICE    PPCBIT(16)
#define HID0_DCE    PPCBIT(17)
#define HID0_ILOCK  PPCBIT(18)
#define HID0_DLOCK  PPCBIT(19)
#define HID0_ICFI   PPCBIT(20)
#define HID0_DCFI   PPCBIT(21)
#define HID0_SPD    PPCBIT(22)
#define HID0_IFEM   PPCBIT(23)
#define HID0_SGE    PPCBIT(24)
#define HID0_DCFA   PPCBIT(25)
#define HID0_BTIC   PPCBIT(26)
#define HID0_ABE    PPCBIT(28)
#define HID0_BHT    PPCBIT(29)
#define HID0_NOOPTI PPCBIT(31)

#define HID2_LSQE   PPCBIT(0)
#define HID2_WPE    PPCBIT(1)
#define HID2_PSE    PPCBIT(2)
#define HID2_LCE    PPCBIT(3)
#define HID2_DCHERR PPCBIT(8)
#define HID2_DNCERR PPCBIT(9)
#define HID2_DCMERR PPCBIT(10)
#define HID2_DQOERR PPCBIT(11)
#define HID2_DCHEE  PPCBIT(12)
#define HID2_DNCEE  PPCBIT(13)
#define HID2_DCMEE  PPCBIT(14)
#define HID2_DQOEE  PPCBIT(15)

#define HID4_H4A    PPCBIT(0)
#define HID4_L2FM(_n) ((_n)<<(31-2))
#define HID4_BPD(_n)  ((_n)<<(31-4))
#define HID4_BCO    PPCBIT(5)
#define HID4_SBE    PPCBIT(6)
#define HID4_ST0    PPCBIT(7)
#define HID4_LPE    PPCBIT(8)
#define HID4_DBP    PPCBIT(9)
#define HID4_L2MUM  PPCBIT(10)
#define HID4_L2CFI  PPCBIT(11)

#define L2CR_L2E    PPCBIT(0)
#define L2CR_L2CE   PPCBIT(1)
#define L2CR_L2DO   PPCBIT(9)
#define L2CR_L2I    PPCBIT(10)
#define L2CR_L2WT   PPCBIT(12)
#define L2CR_L2TS   PPCBIT(13)
#define L2CR_L2IP   PPCBIT(31)
