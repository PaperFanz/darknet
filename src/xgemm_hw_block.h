// ==============================================================
// File generated on Tue Dec 07 13:33:25 CST 2021
// Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
// SW Build 2405991 on Thu Dec  6 23:36:41 MST 2018
// IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// ==============================================================
// AXILiteS
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read)
//        bit 7  - auto_restart (Read/Write)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x10 : Data signal of M
//        bit 31~0 - M[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of N
//        bit 31~0 - N[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of K
//        bit 31~0 - K[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of S
//        bit 31~0 - S[31:0] (Read/Write)
// 0x2c : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XGEMM_AXILITES_ADDR_AP_CTRL 0x00
#define XGEMM_AXILITES_ADDR_GIE     0x04
#define XGEMM_AXILITES_ADDR_IER     0x08
#define XGEMM_AXILITES_ADDR_ISR     0x0c
#define XGEMM_AXILITES_ADDR_M_DATA  0x10
#define XGEMM_AXILITES_BITS_M_DATA  32
#define XGEMM_AXILITES_ADDR_N_DATA  0x18
#define XGEMM_AXILITES_BITS_N_DATA  32
#define XGEMM_AXILITES_ADDR_K_DATA  0x20
#define XGEMM_AXILITES_BITS_K_DATA  32
#define XGEMM_AXILITES_ADDR_S_DATA  0x28
#define XGEMM_AXILITES_BITS_S_DATA  32

