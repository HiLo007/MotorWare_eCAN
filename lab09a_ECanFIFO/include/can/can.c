/*
 * can.c
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * See LICENCE file for LGPL details.
 *
 * Contains the various functions related to the eCAN
 * Target Device:  TMS320F2802x
 * Copyright (C): 2017  Andrew Buckin
 *  Created on: April 20, 2017
 *      Author: Andrew Buckin
 *
 */
#include <stdlib.h>
#include "can.h"
#include "fifo.h"

#define NELEMS(x) (sizeof(x)/sizeof((x)[0]))
//! \brief Define to allow protected register writes (legacy)
//!
#define  EALLOW asm(" EALLOW")

//! \brief Define to allow protected register writes
//!
#define  ENABLE_PROTECTED_REGISTER_WRITE_MODE  asm(" EALLOW")

//! \brief Define to disable protected register writes (legacy)
//!
#define  EDIS   asm(" EDIS")

//! \brief Define to disable protected register writes
//!
#define  DISABLE_PROTECTED_REGISTER_WRITE_MODE asm(" EDIS")

ECAN_Handle ECAN_init(){

	ECAN_Handle handle = (ECAN_Handle)calloc(1, sizeof(ECAN_Obj));

	if(handle == NULL){
		return (ECAN_Handle)NULL;
	}

	handle->ECanaRegs = (ECAN_REGS_t*)ECANA_REGS_ADDR;
	handle->ECanaMboxes = (ECAN_MBOXES_t*)ECANA_MBOX_ADDR;
	handle->ECanaLAMRegs = (LAM_REGS_t*)ECANA_LAM_ADDR;
	handle->ECanaMOTORegs = (MOTO_REGS_t*)ECANA_MOTO_ADDR;
	handle->ECanaMOTSRegs = (MOTS_REGS_t*)ECANA_MOTS_ADDR;

	return handle;
}
void ECAN_setBitrate(ECAN_Handle handle, ECAN_Bitrate_e bitrate){
	struct ECAN_REGS ECanaShadow;

	EALLOW; // Allow access to protected bits

	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.CCR = 1 ;            // Set CCR = 1
	handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;

	// Wait until the CPU has been granted permission to change the configuration registers
	do
	{
	  ECanaShadow.CANES.all = handle->ECanaRegs->CANES.all;
	} while(ECanaShadow.CANES.bit.CCE != 1 );       // Wait for CCE bit to be set..

	ECanaShadow.CANBTC.all = 0;
	// http://www.bittiming.can-wiki.info/   BT=15
	// 90 MHz SYSCLKOUT. (45 MHz CAN module clock)
	switch(bitrate) {
	   case Bitrate_1M:   ECanaShadow.CANBTC.all = 0x00020059; break;
	   case Bitrate_500K: ECanaShadow.CANBTC.all = 0x00050059; break;
	   case Bitrate_250K: ECanaShadow.CANBTC.all = 0x000b0059; break;
	   case Bitrate_125K: ECanaShadow.CANBTC.all = 0x00170059; break;
	   case Bitrate_100K: ECanaShadow.CANBTC.all = 0x001d0059; break;
	   case Bitrate_83K:  ECanaShadow.CANBTC.all = 0x00230059; break;
	   case Bitrate_50K:  ECanaShadow.CANBTC.all = 0x003b0059; break;
	   case Bitrate_20K:  ECanaShadow.CANBTC.all = 0x00950059; break;
	   default:			  ECanaShadow.CANBTC.all = 0x000503BD; break; //500K 100ns  sprac35
		//ECanaShadow.CANBTC.bit.BRPREG = 5;
		//ECanaShadow.CANBTC.bit.SJWREG = 3;
		//ECanaShadow.CANBTC.bit.SAM = 1;
		//ECanaShadow.CANBTC.bit.TSEG1REG = 7;
		//ECanaShadow.CANBTC.bit.TSEG2REG = 5;
	}

	handle->ECanaRegs->CANBTC.all = ECanaShadow.CANBTC.all;

	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.CCR = 0 ;            // Set CCR = 0
	handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;

	// Wait until the CPU no longer has permission to change the configuration registers
	do
	{
	  ECanaShadow.CANES.all = handle->ECanaRegs->CANES.all;
	} while(ECanaShadow.CANES.bit.CCE != 0 );       // Wait for CCE bit to be  cleared..


	EDIS; // Disable access to protected bits
}

void ECAN_setBTCreg(ECAN_Handle handle, long BTC_ALL){
	struct ECAN_REGS ECanaShadow;

	EALLOW; // Allow access to protected bits

	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.CCR = 1 ;            // Set CCR = 1
	handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;

	// Wait until the CPU has been granted permission to change the configuration registers
	do
	{
	  ECanaShadow.CANES.all = handle->ECanaRegs->CANES.all;
	} while(ECanaShadow.CANES.bit.CCE != 1 );       // Wait for CCE bit to be set..

	ECanaShadow.CANBTC.all = BTC_ALL;

	handle->ECanaRegs->CANBTC.all = ECanaShadow.CANBTC.all;

	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.CCR = 0 ;            // Set CCR = 0
	handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;

	// Wait until the CPU no longer has permission to change the configuration registers
	do
	{
	  ECanaShadow.CANES.all = handle->ECanaRegs->CANES.all;
	} while(ECanaShadow.CANES.bit.CCE != 0 );       // Wait for CCE bit to be  cleared..


	EDIS; // Disable access to protected bits
}

void ECAN_enableAllInt(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	//Enable interrupts.
	ECanaShadow.CANGIM.all = 0;
	ECanaShadow.CANGIM.bit.AAIM = 1; /* Abort acknowledge */
	ECanaShadow.CANGIM.bit.WDIM = 1; /* Write denied */
	ECanaShadow.CANGIM.bit.WUIM = 1; /* Wake up */
	ECanaShadow.CANGIM.bit.BOIM = 1; /* Bus-off */
	ECanaShadow.CANGIM.bit.EPIM = 1; /* Error-passive */
	ECanaShadow.CANGIM.bit.WLIM = 1; /* Warning level */
	ECanaShadow.CANGIM.bit.GIL = 1;  /* Global Interrup Level. */ //the GIL (GIM.2) bit can be set to have the global interrupts on another level than the mailbox interrupts
	ECanaShadow.CANGIM.bit.I0EN = 1; /* Interrupt 0 enable */
	ECanaShadow.CANGIM.bit.I1EN = 1; /* Interrupt 1 enable */
	ECanaShadow.CANGIM.bit.RMLIM = 1; /* Received Messae Lost */

	handle->ECanaRegs->CANGIM.all = ECanaShadow.CANGIM.all;
	EDIS; // Disable access to protected bits
}

void ECAN_disableAllInt(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANGIM.all = 0;  //Disable interrupts.
	handle->ECanaRegs->CANGIM.all = ECanaShadow.CANGIM.all;
	EDIS; // Disable access to protected bits
}

void ECAN_clearMSGCTRL(ECAN_Handle handle){
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0));
    int i = 0;
    for(i = 0; i < 32; i++) {
    	mbox->MSGCTRL.all = 0x00000000;
    	//mbox->MSGCTRL.all = i; //Test
    	*mbox++;
    }
}

void ECAN_clearMSGID(ECAN_Handle handle){
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0));
	/* Disable all Mailboxes  */
	handle->ECanaRegs->CANME.all = 0x00000000;        // Required before writing the MSGIDs
    int i = 0;
    for(i = 0; i < 32; i++)     {
    	mbox->MSGID.all = 0x00000000;
    	//mbox->MSGID.all = i; //Test
    	*mbox++;
    }
    //handle->ECanaRegs->CANME.all = 0xFFFFFFFF;
}

void ECAN_clearMDL(ECAN_Handle handle){
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0));
    int i = 0;
    for(i = 0; i < 32; i++)     {
    	mbox->MDL.all = 0x00000000;
    	//mbox->MDL.all = i; //Test
    	*mbox++;
    }
}

void ECAN_clearMDH(ECAN_Handle handle){
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0));
    int i = 0;
    for(i = 0; i < 32; i++) {
    	mbox->MDH.all = 0x00000000;
    	//mbox->MDH.all = i; //Test
    	*mbox++;
    }
}

/* enable all Mailboxes  */
void ECAN_enableAllMailbox(ECAN_Handle handle){
	handle->ECanaRegs->CANME.all = 0xFFFFFFFF;
}

/* disable all Mailboxes  */
void ECAN_disableAllMailbox(ECAN_Handle handle){
	handle->ECanaRegs->CANME.all = 0x00000000;
}

void ECAN_setTXIO(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANTIOC.all = handle->ECanaRegs->CANTIOC.all;
	ECanaShadow.CANTIOC.bit.TXFUNC = 1;
	handle->ECanaRegs->CANTIOC.all = ECanaShadow.CANTIOC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_setRXIO(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANRIOC.all = handle->ECanaRegs->CANRIOC.all;
	ECanaShadow.CANRIOC.bit.RXFUNC = 1;
	handle->ECanaRegs->CANRIOC.all = ECanaShadow.CANRIOC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_resetTXIO(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANTIOC.all = handle->ECanaRegs->CANTIOC.all;
	ECanaShadow.CANTIOC.bit.TXFUNC = 0;
	handle->ECanaRegs->CANTIOC.all = ECanaShadow.CANTIOC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_resetRXIO(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANRIOC.all = handle->ECanaRegs->CANRIOC.all;
	ECanaShadow.CANRIOC.bit.RXFUNC = 0;
	handle->ECanaRegs->CANRIOC.all = ECanaShadow.CANRIOC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_setSCCmode(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	/* Configure eCAN for HECC mode - (reqd to access mailboxes 16 thru 31) SCC mode */
										// HECC mode also enables time-stamping feature
	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.SCB = 1;
	handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_setECANmode(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.SCB = 0;
	handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_Mode(ECAN_Handle handle, SCB_Bit_e mode){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.SCB = mode;
	handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_setSelfTest(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.STM = 1;    // Enable self-test mode.
    handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_resetSelfTest(ECAN_Handle handle){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.STM = 0;    // Disable self-test mode.
    handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;
	EDIS; // Disable access to protected bits
}

void ECAN_SelfTest(ECAN_Handle handle,STM_Bit_e mode){
	struct ECAN_REGS ECanaShadow;
	EALLOW; // Allow access to protected bits
	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.STM = mode;    // Disable self-test mode.
    handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;
	EDIS; // Disable access to protected bits
}

/* Clear all Transmission-Acknowledge Register bits */
void ECAN_clearCANTA(ECAN_Handle handle){
	handle->ECanaRegs->CANTA.all = 0xFFFFFFFF;
}

/* Clear all Received-Message-Pending Register bits */
void ECAN_clearCANRMP(ECAN_Handle handle){
	handle->ECanaRegs->CANRMP.all = 0xFFFFFFFF;
}

/* Clear all Global Interrupt Flag 0 */
void ECAN_clearCANGIF0(ECAN_Handle handle){
	handle->ECanaRegs->CANGIF0.all = 0xFFFFFFFF;
}

/* Clear all Global Interrupt Flag 1 */
void ECAN_clearCANGIF1(ECAN_Handle handle){
	handle->ECanaRegs->CANGIF1.all = 0xFFFFFFFF;
}

/* Mailbox-Direction Register  */
void ECAN_setMailboxDir(ECAN_Handle handle,long dir){
	handle->ECanaRegs->CANMD.all = dir;
}

/* Mailbox Interrupt Mask Register  */
void ECAN_setMailboxIntMask(ECAN_Handle handle,long mask){
	EALLOW; // Allow access to protected bits
	handle->ECanaRegs->CANMIM.all = mask;
	EDIS; // Disable access to protected bits
}

void ECAN_configMailbox(ECAN_Handle handle, ECAN_MailBox_e MailBoxN, uint32_t msgid, Enable_Mbox_e enable_t, ECAN_MailDir_e dir_t,IDE_Bit_e IDE_t, DLC_Bit_e length, OPC_Bit_e opc_t, LAMI_Bit_e lami_bit, AME_Bit_e AME_t, uint32_t mask){
	struct ECAN_REGS ECanaShadow;
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0)) + /*(uint32_t)*/MailBoxN;
	volatile union CANLAM_REG *lam = (&(handle->ECanaLAMRegs->LAM0)) + /*(uint32_t)*/MailBoxN;
	uint8_t enable = enable_t;
	uint8_t dir = dir_t;
	uint8_t opc = opc_t;

	ECanaShadow.CANME.all = handle->ECanaRegs->CANME.all;
	ECanaShadow.CANME.all = (~((~ECanaShadow.CANME.all) | (((uint32_t)1) << MailBoxN))) | (((uint32_t)Disable_Mbox) << MailBoxN);
	handle->ECanaRegs->CANME.all = ECanaShadow.CANME.all;

	lam->all = mask;
	lam->bit.LAMI = lami_bit;

	if (IDE_t == Extended_ID)
	{
		mbox->MSGID.all = msgid;
	}
	else
	{
		mbox->MSGID.bit.EXTMSGID_L = 0;
		mbox->MSGID.bit.EXTMSGID_H = 0;
		mbox->MSGID.bit.STDMSGID = (uint16_t)msgid;
	}

	mbox->MSGID.bit.IDE = IDE_t;
	mbox->MSGID.bit.AME = AME_t;
	mbox->MSGID.bit.AAM = Normal_transmit;

    // Specify that bits will be sent/received
    mbox->MSGCTRL.bit.DLC = length; //8;

    //Enable/disable mbox.
	enable = enable & 1; //Making sure it's only one bit.
	ECanaShadow.CANME.all = handle->ECanaRegs->CANME.all;
	ECanaShadow.CANME.all = (~((~ECanaShadow.CANME.all) | (((uint32_t)1) << MailBoxN))) | (((uint32_t)enable) << MailBoxN);
	handle->ECanaRegs->CANME.all = ECanaShadow.CANME.all;

	//Setting rx/tx mode.
	dir = dir & 1; //Making sure it's only one bit.
	ECanaShadow.CANMD.all = handle->ECanaRegs->CANMD.all;
	ECanaShadow.CANMD.all = (~((~ECanaShadow.CANMD.all) | (((uint32_t)1) << MailBoxN))) | (((uint32_t)dir) << MailBoxN);
	handle->ECanaRegs->CANMD.all = ECanaShadow.CANMD.all;

	//Overwrite Protection
	opc = opc & 1; //Making sure it's only one bit.
	ECanaShadow.CANOPC.all = handle->ECanaRegs->CANOPC.all;
	ECanaShadow.CANOPC.all = (~((~ECanaShadow.CANOPC.all) | (((uint32_t)1) << MailBoxN))) | (((uint32_t)opc) << MailBoxN);
	handle->ECanaRegs->CANOPC.all = ECanaShadow.CANOPC.all;
}

void ECAN_putDataMailbox(ECAN_Handle handle,ECAN_MailBox_e MailBoxN, long MDL_t, long MDH_t){
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0)) + /*(uint32_t)*/MailBoxN;
	mbox->MDL.all = MDL_t;
	mbox->MDH.all = MDH_t;
}

/* config Master Control Register  */
void ECAN_configMasterReg(ECAN_Handle handle, CCR_Bit_e CCR_t, PDR_Bit_e PDR_t, DBO_Bit_e DBO_t, WUBA_Bit_e WUBA_t, CDR_Bit_e CDR_t, ABO_Bit_e ABO_t, SRES_Bit_e SRES_t, MBNR_Bit_e MBNR_t){
	EALLOW; // Allow access to protected bits
	struct ECAN_REGS ECanaShadow;
	ECanaShadow.CANMC.all = handle->ECanaRegs->CANMC.all;
	ECanaShadow.CANMC.bit.CCR = CCR_t;
	ECanaShadow.CANMC.bit.PDR = PDR_t;
	ECanaShadow.CANMC.bit.DBO = DBO_t;
	ECanaShadow.CANMC.bit.WUBA = WUBA_t;
	ECanaShadow.CANMC.bit.CDR = CDR_t;
	ECanaShadow.CANMC.bit.ABO = ABO_t;
	ECanaShadow.CANMC.bit.SRES = SRES_t;
	ECanaShadow.CANMC.bit.MBNR = MBNR_t;
	handle->ECanaRegs->CANMC.all = ECanaShadow.CANMC.all;
	EDIS; // Disable access to protected bits
}
// Sends data to the MailBox_N.
int ECAN_sendMsg(ECAN_Handle handle,ECAN_MailBox_e MailBoxN, long MDL_t, long MDH_t){
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0)) + MailBoxN;

	mbox->MDL.all = MDL_t;
	mbox->MDH.all = MDH_t;

    handle->ECanaRegs->CANTRS.all  |= 1 << MailBoxN;
    //Wait for transmit acknowledge
    while(!(handle->ECanaRegs->CANTA.all >> MailBoxN));
    //Wait TRS.n = 0
    while(handle->ECanaRegs->CANTRS.all >> MailBoxN);
    //Set TA.n
    handle->ECanaRegs->CANTA.all |= 1 << MailBoxN;
    //Wait until read TA.n is 0
    while(handle->ECanaRegs->CANTA.all >> MailBoxN);
    return 0;
}

// init MailBox struct. It is necessary to work with the MailBox - queue and FIFO
void ECAN_initMailboxObj(ECAN_Mailbox *pECAN_Mailbox, ECAN_MailBox_e TX_max_t, ECAN_MailBox_e TX_min_t, ECAN_MailBox_e RX_max_t, ECAN_MailBox_e RX_min_t){

	pECAN_Mailbox->TX_last = TX_max_t;
	pECAN_Mailbox->TX_max = TX_max_t;
	pECAN_Mailbox->TX_min = TX_min_t;
	pECAN_Mailbox->TX_ind = 0;
	pECAN_Mailbox->RX_last = RX_max_t;
	pECAN_Mailbox->RX_max = RX_max_t;
	pECAN_Mailbox->RX_min = RX_min_t;
	pECAN_Mailbox->RX_ind = 0;
}
// Sends data to the next MailBox.
int ECAN_sendMsg_N(ECAN_Handle handle, ECAN_Mailbox *pECAN_Mailbox, long MDL_t, long MDH_t){

	if(pECAN_Mailbox->TX_ind){
		pECAN_Mailbox->TX_ind = 0;
		pECAN_Mailbox->TX_last = pECAN_Mailbox->TX_max;
	    while(!(handle->ECanaRegs->CANTA.all));     // Wait for transmit acknowledge
	    while(handle->ECanaRegs->CANTRS.all);       // Wait TRS.n = 0
	    handle->ECanaRegs->CANTA.all |= 0xFFFFFFFF; // Set TA.n
	    while(handle->ECanaRegs->CANTA.all);        // Wait until read TA.n is 0
	}

	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0)) + pECAN_Mailbox->TX_last;
	mbox->MDL.all = MDL_t;
	mbox->MDH.all = MDH_t;
    handle->ECanaRegs->CANTRS.all  |= 1 << pECAN_Mailbox->TX_last;

    if(pECAN_Mailbox->TX_last == pECAN_Mailbox->TX_min){
    	pECAN_Mailbox->TX_ind = 1;
    }
    pECAN_Mailbox->TX_last--;

    return 0;
}

bool ECAN_checkMail(ECAN_Handle handle){
    if (handle->ECanaRegs->CANRMP.all == 0) return false;
    else return true;
}
uint32_t ECAN_getRMP(ECAN_Handle handle){
    return handle->ECanaRegs->CANRMP.all;
}

uint32_t ECAN_getRML(ECAN_Handle handle){
	return handle->ECanaRegs->CANRML.all;
}

uint32_t ECAN_getOPC(ECAN_Handle handle){
	return handle->ECanaRegs->CANOPC.all;
}

// Reads one MailBox to Buf
bool ECAN_getMsg(ECAN_Handle handle,ECAN_MailBox_e MailBoxN, uint32_t *MDL_t, uint32_t *MDH_t) {
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0)) + MailBoxN;

    if((((1 << MailBoxN) & handle->ECanaRegs->CANRMP.all) >> MailBoxN) == 0)
    {
        return false;
    }
    else
    {
        //Loop as long as this mailbox has a msg
        while((((1 << MailBoxN) & handle->ECanaRegs->CANRMP.all) >> MailBoxN) == 1){

        	handle->ECanaRegs->CANRMP.all |= 1 << MailBoxN;
        }
	*MDL_t = mbox->MDL.all;
	*MDH_t = mbox->MDH.all;
	return true;
    }
}

// Reads one MailBox to FIFO
bool ECAN_getMsgFIFO(ECAN_Handle handle,ECAN_MailBox_e MailBoxN, FIFO_Obj *pECAN_rxFIFO) {
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0)) + MailBoxN;
    if((((1 << MailBoxN) & handle->ECanaRegs->CANRMP.all) >> MailBoxN) == 0)
    {
        return false;
    }
    else
    {
        //Loop as long as this mailbox has a msg
        while((((1 << MailBoxN) & handle->ECanaRegs->CANRMP.all) >> MailBoxN) == 1){

        	handle->ECanaRegs->CANRMP.all |= 1 << MailBoxN;
        }
    FIFO_PUSH ( *pECAN_rxFIFO, mbox->MDL.all);
    FIFO_PUSH ( *pECAN_rxFIFO, mbox->MDH.all);
	return true;
    }
}
// Reads all MailBox to FIFO
bool ECAN_getMsgFIFO_N(ECAN_Handle handle, ECAN_Mailbox *pECAN_Mailbox, FIFO_Obj *pECAN_rxFIFO) {
	struct ECAN_REGS ECanaShadow;
	volatile struct MBOX *mbox = (&(handle->ECanaMboxes->MBOX0)) + pECAN_Mailbox->RX_max;
	pECAN_Mailbox->RX_last = pECAN_Mailbox->RX_max;
	uint32_t rmp = (uint32_t)handle->ECanaRegs->CANRMP.all;

	while(rmp != 0) {

		ECanaShadow.CANRMP.all = handle->ECanaRegs->CANRMP.all;
		ECanaShadow.CANRMP.all = (~((~ECanaShadow.CANRMP.all) | (((uint32_t)1) << pECAN_Mailbox->RX_last))) | (((uint32_t)0) << pECAN_Mailbox->RX_last);
		handle->ECanaRegs->CANRMP.all = ECanaShadow.CANRMP.all;

        FIFO_PUSH ( *pECAN_rxFIFO, mbox->MDL.all);
        FIFO_PUSH ( *pECAN_rxFIFO, mbox->MDH.all);
        rmp<<=1;
        pECAN_Mailbox->RX_last--;
        *mbox--;
		}
	rmp = 0;
	pECAN_Mailbox->RX_last = pECAN_Mailbox->RX_max;
	return true;
}

// Returns the count of the received mailbox.
uint8_t ECAN_isRx(ECAN_Handle handle){ //, ECAN_Mailbox *pECAN_Mailbox
	uint32_t rmp;
	uint8_t k = 0;
	rmp = (uint32_t)handle->ECanaRegs->CANRMP.all;
	while(rmp) {
		rmp<<=1;
		k++;
	}
    return k;
}

