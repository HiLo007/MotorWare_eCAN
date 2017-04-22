/* --COPYRIGHT--,BSD
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//! \file   solutions/instaspin_foc/src/proj_lab09a.c
//! \brief Automatic field weakening FPU32
//!
//! (C) Copyright 2011, Texas Instruments, Inc.

//! \defgroup PROJ_LAB09A PROJ_LAB09A
//@{

//! \defgroup PROJ_LAB09A_OVERVIEW Project Overview
//!
//! Experimentation with Field Weakening FPU32
//!
//! Added ECanBack2Back  Andrew Buckin.
//!

// **************************************************************************
// the includes

// system includes
#include <math.h>
#include "main.h"

#ifdef FLASH
#pragma CODE_SECTION(mainISR,"ramfuncs");
#endif

#include "can.h"
#include "fifo.h"

// Include header files used in the main function


// **************************************************************************
// the defines

#define LED_BLINK_FREQ_Hz   5


// **************************************************************************
// the globals

uint_least16_t gCounter_updateGlobals = 0;

bool Flag_Latch_softwareUpdate = true;

CTRL_Handle ctrlHandle;

#ifdef CSM_ENABLE
#pragma DATA_SECTION(halHandle,"rom_accessed_data");
#endif

HAL_Handle halHandle;

#ifdef CSM_ENABLE
#pragma DATA_SECTION(gUserParams,"rom_accessed_data");
#endif

USER_Params gUserParams;

HAL_PwmData_t gPwmData = {_IQ(0.0), _IQ(0.0), _IQ(0.0)};

HAL_AdcData_t gAdcData;

_iq gMaxCurrentSlope = _IQ(0.0);

#ifdef FAST_ROM_V1p6
CTRL_Obj *controller_obj;
#else

#ifdef CSM_ENABLE
#pragma DATA_SECTION(ctrl,"rom_accessed_data");
#endif

CTRL_Obj ctrl;				//v1p7 format
#endif

uint16_t gLEDcnt = 0;
uint16_t gTXtemp = 0;

uint32_t timer0_count=0;

int ECANIDS = 6;
uint32_t ECAN_rxBuf[8] = {0,0,0,0,0,0,0,0};
ECAN_Mailbox gECAN_Mailbox;

//FIFO(32) gECAN_rxFIFO;

FIFO_Obj gECAN_rxFIFO;


volatile MOTOR_Vars_t gMotorVars = MOTOR_Vars_INIT;

#ifdef FLASH
// Used for running BackGround in flash, and ISR in RAM
extern uint16_t *RamfuncsLoadStart, *RamfuncsLoadEnd, *RamfuncsRunStart;

#ifdef CSM_ENABLE
extern uint16_t *econst_start, *econst_end, *econst_ram_load;
extern uint16_t *switch_start, *switch_end, *switch_ram_load;
#endif
#endif

FW_Obj fw;
FW_Handle fwHandle;

_iq Iq_Max_pu;


#ifdef DRV8301_SPI
// Watch window interface to the 8301 SPI
DRV_SPI_8301_Vars_t gDrvSpi8301Vars;
#endif
#ifdef DRV8305_SPI
// Watch window interface to the 8305 SPI
DRV_SPI_8305_Vars_t gDrvSpi8305Vars;
#endif

_iq gFlux_pu_to_Wb_sf;

_iq gFlux_pu_to_VpHz_sf;

_iq gTorque_Ls_Id_Iq_pu_to_Nm_sf;

_iq gTorque_Flux_Iq_pu_to_Nm_sf;

// **************************************************************************
// the functions
#ifdef ecan_test
uint_least32_t  ErrorCount = 0;
uint_least32_t  PassCount = 0;
uint_least32_t  MessageReceivedCount = 0;

uint_least32_t  TestMbox1 = 0;
uint_least32_t  TestMbox2 = 0;
uint_least32_t  TestMbox3 = 0;

uint_least16_t  j;

// Prototype statements for functions found within this file.
void mailbox_check(int_least32_t T1, int_least32_t T2, int_least32_t T3);
void mailbox_read(ECAN_Handle handle, int_least16_t MBXnbr);
#endif

#ifdef ECAN_API
uint_least32_t  ErrorCount = 0;
uint_least32_t  PassCount = 0;
uint_least32_t  MessageReceivedCount = 0;
#endif

void main(void)
{
  uint_least8_t estNumber = 0;

#ifdef FAST_ROM_V1p6
  uint_least8_t ctrlNumber = 0;
#endif

  FIFO_FLUSH(gECAN_rxFIFO);

  // Only used if running from FLASH
  // Note that the variable FLASH is defined by the project
  #ifdef FLASH
  // Copy time critical code and Flash setup code to RAM
  // The RamfuncsLoadStart, RamfuncsLoadEnd, and RamfuncsRunStart
  // symbols are created by the linker. Refer to the linker files.
  memCopy((uint16_t *)&RamfuncsLoadStart,(uint16_t *)&RamfuncsLoadEnd,(uint16_t *)&RamfuncsRunStart);

  #ifdef CSM_ENABLE
  //copy .econst to unsecure RAM
  if(*econst_end - *econst_start)
  {
     memCopy((uint16_t *)&econst_start,(uint16_t *)&econst_end,(uint16_t *)&econst_ram_load);
  }

  //copy .switch ot unsecure RAM
  if(*switch_end - *switch_start)
  {
    memCopy((uint16_t *)&switch_start,(uint16_t *)&switch_end,(uint16_t *)&switch_ram_load);
  }
  #endif
  #endif

  // initialize the hardware abstraction layer
  halHandle = HAL_init(&hal,sizeof(hal));

  // check for errors in user parameters
  USER_checkForErrors(&gUserParams);


  // store user parameter error in global variable
  gMotorVars.UserErrorCode = USER_getErrorCode(&gUserParams);


  // do not allow code execution if there is a user parameter error
  if(gMotorVars.UserErrorCode != USER_ErrorCode_NoError)
    {
      for(;;)
        {
          gMotorVars.Flag_enableSys = false;
        }
    }


  // initialize the user parameters
  USER_setParams(&gUserParams);


  // set the hardware abstraction layer parameters
  HAL_setParams(halHandle,&gUserParams);


  // initialize the controller
#ifdef FAST_ROM_V1p6
  ctrlHandle = CTRL_initCtrl(ctrlNumber, estNumber);  		//v1p6 format (06xF and 06xM devices)
  controller_obj = (CTRL_Obj *)ctrlHandle;
#else
  ctrlHandle = CTRL_initCtrl(estNumber,&ctrl,sizeof(ctrl));	//v1p7 format default
#endif


  {
    CTRL_Version version;

    // get the version number
    CTRL_getVersion(ctrlHandle,&version);

    gMotorVars.CtrlVersion = version;
  }


  // set the default controller parameters
  CTRL_setParams(ctrlHandle,&gUserParams);


  // Initialize field weakening
  fwHandle = FW_init(&fw,sizeof(fw));


  // Disable field weakening
  FW_setFlag_enableFw(fwHandle, false);


  // Clear field weakening counter
  FW_clearCounter(fwHandle);


  // Set the number of ISR per field weakening ticks
  FW_setNumIsrTicksPerFwTick(fwHandle, FW_NUM_ISR_TICKS_PER_CTRL_TICK);


  // Set the deltas of field weakening
  FW_setDeltas(fwHandle, FW_INC_DELTA, FW_DEC_DELTA);


  // Set initial output of field weakening to zero
  FW_setOutput(fwHandle, _IQ(0.0));


  // Set the field weakening controller limits
  FW_setMinMax(fwHandle,_IQ(USER_MAX_NEGATIVE_ID_REF_CURRENT_A/USER_IQ_FULL_SCALE_CURRENT_A),_IQ(0.0));


  // setup faults
  HAL_setupFaults(halHandle);


  // initialize the interrupt vector table
  HAL_initIntVectorTable(halHandle);


  // enable the ADC interrupts
  HAL_enableAdcInts(halHandle);


  // enable global interrupts
  HAL_enableGlobalInts(halHandle);


  // enable debug interrupts
  HAL_enableDebugInt(halHandle);


  // disable the PWM
  HAL_disablePwm(halHandle);

  // enable the Timer 0 interrupts
  HAL_enableTimer0Int(halHandle);


#ifdef DRV8301_SPI
  // turn on the DRV8301 if present
  HAL_enableDrv(halHandle);
  // initialize the DRV8301 interface
  HAL_setupDrvSpi(halHandle,&gDrvSpi8301Vars);
#endif

#ifdef DRV8305_SPI
  // turn on the DRV8305 if present
  HAL_enableDrv(halHandle);
  // initialize the DRV8305 interface
  HAL_setupDrvSpi(halHandle,&gDrvSpi8305Vars);
#endif

  // enable DC bus compensation
  CTRL_setFlag_enableDcBusComp(ctrlHandle, true);


  // compute scaling factors for flux and torque calculations
  gFlux_pu_to_Wb_sf = USER_computeFlux_pu_to_Wb_sf();
  gFlux_pu_to_VpHz_sf = USER_computeFlux_pu_to_VpHz_sf();
  gTorque_Ls_Id_Iq_pu_to_Nm_sf = USER_computeTorque_Ls_Id_Iq_pu_to_Nm_sf();
  gTorque_Flux_Iq_pu_to_Nm_sf = USER_computeTorque_Flux_Iq_pu_to_Nm_sf();

#ifdef ecan_test
  //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  //                                                    MailBoxID                                                                   Mask
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox0,  0x1555AAA0, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox1,  0x1555AAA1, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox2,  0x1555AAA2, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox3,  0x1555AAA3, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox4,  0x1555AAA4, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox5,  0x1555AAA5, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox6,  0x1555AAA6, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox7,  0x1555AAA7, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox8,  0x1555AAA8, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox9,  0x1555AAA9, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox10, 0x1555AAAA, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox11, 0x1555AAAB, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox12, 0x1555AAAC, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox13, 0x1555AAAD, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox14, 0x1555AAAE, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox15, 0x1555AAAF, Enable_Mbox, Tx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);

  ECAN_configMailbox(halHandle->ecanaHandle, MailBox16, 0x1555AAA0, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox17, 0x1555AAA1, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox18, 0x1555AAA2, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox19, 0x1555AAA3, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox20, 0x1555AAA4, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox21, 0x1555AAA5, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox22, 0x1555AAA6, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox23, 0x1555AAA7, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox24, 0x1555AAA8, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox25, 0x1555AAA9, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox26, 0x1555AAAA, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox27, 0x1555AAAB, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox28, 0x1555AAAC, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox29, 0x1555AAAD, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox30, 0x1555AAAE, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox31, 0x1555AAAF, Enable_Mbox, Rx_Dir, Extended_ID, DLC_8, LAMI_0, Mask_not_used, 0x00000000);
//                                                       MDL         MDH
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox0,  0x9555AAA0, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox1,  0x9555AAA1, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox2,  0x9555AAA2, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox3,  0x9555AAA3, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox4,  0x9555AAA4, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox5,  0x9555AAA5, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox6,  0x9555AAA6, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox7,  0x9555AAA7, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox8,  0x9555AAA8, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox9,  0x9555AAA9, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox10, 0x9555AAAA, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox11, 0x9555AAAB, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox12, 0x9555AAAC, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox13, 0x9555AAAD, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox14, 0x9555AAAE, 0x89ABCDEF);
  ECAN_putDataMailbox(halHandle->ecanaHandle, MailBox15, 0x9555AAAF, 0x89ABCDEF);

  //ECAN_setSelfTest(halHandle->ecanaHandle);
  //ECAN_resetSelfTest(halHandle->ecanaHandle);
  //ECAN_SelfTest(halHandle->ecanaHandle, Normal_mode);
  ECAN_SelfTest(halHandle->ecanaHandle, Self_test_mode);
  //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#endif

//                                               	    MailBoxID                                                              Mask
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox0,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox1,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox2,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox3,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox4,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox5,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox6,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox7,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox8,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox9,  ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox10, ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox11, ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox12, ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox13, ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox14, ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox15, ECANIDS, Enable_Mbox, Tx_Dir, Standard_ID, DLC_8, Overwrite_on, LAMI_0, Mask_is_used, 0x0000FF80);

  ECAN_configMailbox(halHandle->ecanaHandle, MailBox16, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox17, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox18, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox19, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox20, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox21, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox22, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox23, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox24, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox25, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox26, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox27, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox28, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox29, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox30, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  ECAN_configMailbox(halHandle->ecanaHandle, MailBox31, ECANIDS, Enable_Mbox, Rx_Dir, Standard_ID, DLC_8, Overwrite_off, LAMI_0, Mask_is_used, 0x0000FF80);
  //                                    TX_max    TX_min    RX_max     RX_min
  ECAN_initMailboxObj(&gECAN_Mailbox, MailBox15, MailBox0, MailBox31, MailBox16);

  ECAN_SelfTest(halHandle->ecanaHandle, Self_test_mode);

  for(;;)
  {
    // Waiting for enable system flag to be set
    while(!(gMotorVars.Flag_enableSys));

    Flag_Latch_softwareUpdate = true;

    // Enable the Library internal PI.  Iq is referenced by the speed PI now
    CTRL_setFlag_enableSpeedCtrl(ctrlHandle, true);

    // loop while the enable system flag is true
    while(gMotorVars.Flag_enableSys)
      {

#ifdef ecan_test
	 ECAN_REGS_t *regs = (halHandle->ecanaHandle->ECanaRegs);
	 regs->CANTRS.all = 0x0000FFFF; // Set TRS for all transmit mailboxes
	 while(regs->CANTA.all != 0x0000FFFF ) {}  // Wait for all TAn bits to be set..
	 regs->CANTA.all = 0x0000FFFF;   // Clear all TAn
	 MessageReceivedCount++;
     //Read from Receive mailboxes and begin checking for data */
     for(j=16; j<32; j++)         // Read & check 16 mailboxes
     {
        mailbox_read(halHandle->ecanaHandle,j);         // This func reads the indicated mailbox data
        mailbox_check(TestMbox1,TestMbox2,TestMbox3); // Checks the received data
     }
#endif

#ifdef ECAN_API

     int i = 0;
     for(i = 0; i < 16; i++) {
    	 ECAN_sendMsg_N(halHandle->ecanaHandle, &gECAN_Mailbox, i+gTXtemp, 0x89ABCDEF);
    	 MessageReceivedCount++;
     }

     if(ECAN_isRx(halHandle->ecanaHandle) == 16){
    	 ECAN_getMsgFIFO_N(halHandle->ecanaHandle, &gECAN_Mailbox, &gECAN_rxFIFO);
    	 FIFO_FLUSH(gECAN_rxFIFO);
     }
     for(i = 0; i < 16; i++) {
    	 ECAN_rxBuf[0] = FIFO_FRONT(gECAN_rxFIFO);
    	 FIFO_POP(gECAN_rxFIFO);
    	 ECAN_rxBuf[1] = FIFO_FRONT(gECAN_rxFIFO);
    	 FIFO_POP(gECAN_rxFIFO);

    	    if((ECAN_rxBuf[0] == i+gTXtemp) || ( ECAN_rxBuf[1] == 0x89ABCDEF)) {
    	    	PassCount++;
    	    } else {
    	    	ErrorCount++;
    	    }
    	     gTXtemp++;
     }



#endif
    	CTRL_Obj *obj = (CTRL_Obj *)ctrlHandle;

        // increment counters
        gCounter_updateGlobals++;

        // enable/disable the use of motor parameters being loaded from user.h
        CTRL_setFlag_enableUserMotorParams(ctrlHandle,gMotorVars.Flag_enableUserParams);

        // enable/disable Rs recalibration during motor startup
        EST_setFlag_enableRsRecalc(obj->estHandle,gMotorVars.Flag_enableRsRecalc);

        // enable/disable automatic calculation of bias values
        CTRL_setFlag_enableOffset(ctrlHandle,gMotorVars.Flag_enableOffsetcalc);


        if(CTRL_isError(ctrlHandle))
          {
            // set the enable controller flag to false
            CTRL_setFlag_enableCtrl(ctrlHandle,false);

            // set the enable system flag to false
            gMotorVars.Flag_enableSys = false;

            // disable the PWM
            HAL_disablePwm(halHandle);
          }
        else
          {
            // update the controller state
            bool flag_ctrlStateChanged = CTRL_updateState(ctrlHandle);

            // enable or disable the control
            CTRL_setFlag_enableCtrl(ctrlHandle, gMotorVars.Flag_Run_Identify);

            if(flag_ctrlStateChanged)
              {
                CTRL_State_e ctrlState = CTRL_getState(ctrlHandle);

                if(ctrlState == CTRL_State_OffLine)
                  {
                    // enable the PWM
                    HAL_enablePwm(halHandle);
                  }
                else if(ctrlState == CTRL_State_OnLine)
                  {
                    if(gMotorVars.Flag_enableOffsetcalc == true)
                    {
                      // update the ADC bias values
                      HAL_updateAdcBias(halHandle);
                    }
                    else
                    {
                      // set the current bias
                      HAL_setBias(halHandle,HAL_SensorType_Current,0,_IQ(I_A_offset));
                      HAL_setBias(halHandle,HAL_SensorType_Current,1,_IQ(I_B_offset));
                      HAL_setBias(halHandle,HAL_SensorType_Current,2,_IQ(I_C_offset));

                      // set the voltage bias
                      HAL_setBias(halHandle,HAL_SensorType_Voltage,0,_IQ(V_A_offset));
                      HAL_setBias(halHandle,HAL_SensorType_Voltage,1,_IQ(V_B_offset));
                      HAL_setBias(halHandle,HAL_SensorType_Voltage,2,_IQ(V_C_offset));
                    }

                    // Return the bias value for currents
                    gMotorVars.I_bias.value[0] = HAL_getBias(halHandle,HAL_SensorType_Current,0);
                    gMotorVars.I_bias.value[1] = HAL_getBias(halHandle,HAL_SensorType_Current,1);
                    gMotorVars.I_bias.value[2] = HAL_getBias(halHandle,HAL_SensorType_Current,2);

                    // Return the bias value for voltages
                    gMotorVars.V_bias.value[0] = HAL_getBias(halHandle,HAL_SensorType_Voltage,0);
                    gMotorVars.V_bias.value[1] = HAL_getBias(halHandle,HAL_SensorType_Voltage,1);
                    gMotorVars.V_bias.value[2] = HAL_getBias(halHandle,HAL_SensorType_Voltage,2);

                    // enable the PWM
                    HAL_enablePwm(halHandle);
                  }
                else if(ctrlState == CTRL_State_Idle)
                  {
                    // disable the PWM
                    HAL_disablePwm(halHandle);
                    gMotorVars.Flag_Run_Identify = false;
                  }

                if((CTRL_getFlag_enableUserMotorParams(ctrlHandle) == true) &&
                  (ctrlState > CTRL_State_Idle) &&
                  (gMotorVars.CtrlVersion.minor == 6))
                  {
                    // call this function to fix 1p6
                    USER_softwareUpdate1p6(ctrlHandle);
                  }

              }
          }


        if(EST_isMotorIdentified(obj->estHandle))
          {
            _iq Is_Max_squared_pu = _IQ((USER_MOTOR_MAX_CURRENT*USER_MOTOR_MAX_CURRENT)/  \
    	      			  (USER_IQ_FULL_SCALE_CURRENT_A*USER_IQ_FULL_SCALE_CURRENT_A));
            _iq Id_squared_pu = _IQmpy(CTRL_getId_ref_pu(ctrlHandle),CTRL_getId_ref_pu(ctrlHandle));

            // Take into consideration that Iq^2+Id^2 = Is^2
            Iq_Max_pu = _IQsqrt(Is_Max_squared_pu-Id_squared_pu);

            //Set new max trajectory
            CTRL_setSpdMax(ctrlHandle, Iq_Max_pu);

            // set the current ramp
            EST_setMaxCurrentSlope_pu(obj->estHandle,gMaxCurrentSlope);
            gMotorVars.Flag_MotorIdentified = true;

            // set the speed reference
            CTRL_setSpd_ref_krpm(ctrlHandle,gMotorVars.SpeedRef_krpm);

            // set the speed acceleration
            CTRL_setMaxAccel_pu(ctrlHandle,_IQmpy(MAX_ACCEL_KRPMPS_SF,gMotorVars.MaxAccel_krpmps));

            if(Flag_Latch_softwareUpdate)
            {
              Flag_Latch_softwareUpdate = false;

              USER_calcPIgains(ctrlHandle);

              // initialize the watch window kp and ki current values with pre-calculated values
              gMotorVars.Kp_Idq = CTRL_getKp(ctrlHandle,CTRL_Type_PID_Id);
              gMotorVars.Ki_Idq = CTRL_getKi(ctrlHandle,CTRL_Type_PID_Id);
            }

          }
        else
          {
            Flag_Latch_softwareUpdate = true;

            // initialize the watch window kp and ki values with pre-calculated values
            gMotorVars.Kp_spd = CTRL_getKp(ctrlHandle,CTRL_Type_PID_spd);
            gMotorVars.Ki_spd = CTRL_getKi(ctrlHandle,CTRL_Type_PID_spd);


            // the estimator sets the maximum current slope during identification
            gMaxCurrentSlope = EST_getMaxCurrentSlope_pu(obj->estHandle);
          }


        // when appropriate, update the global variables
        if(gCounter_updateGlobals >= NUM_MAIN_TICKS_FOR_GLOBAL_VARIABLE_UPDATE)
          {
            // reset the counter
            gCounter_updateGlobals = 0;

            updateGlobalVariables_motor(ctrlHandle);
          }


        // update Kp and Ki gains
        updateKpKiGains(ctrlHandle);

        // set field weakening enable flag depending on user's input
        FW_setFlag_enableFw(fwHandle,gMotorVars.Flag_enableFieldWeakening);

        // enable/disable the forced angle
        EST_setFlag_enableForceAngle(obj->estHandle,gMotorVars.Flag_enableForceAngle);

        // enable or disable power warp
        CTRL_setFlag_enablePowerWarp(ctrlHandle,gMotorVars.Flag_enablePowerWarp);

#ifdef DRV8301_SPI
        HAL_writeDrvData(halHandle,&gDrvSpi8301Vars);

        HAL_readDrvData(halHandle,&gDrvSpi8301Vars);
#endif
#ifdef DRV8305_SPI
        HAL_writeDrvData(halHandle,&gDrvSpi8305Vars);

        HAL_readDrvData(halHandle,&gDrvSpi8305Vars);
#endif

      } // end of while(gFlag_enableSys) loop


    // disable the PWM
    HAL_disablePwm(halHandle);

    // set the default controller parameters (Reset the control to re-identify the motor)
    CTRL_setParams(ctrlHandle,&gUserParams);
    gMotorVars.Flag_Run_Identify = false;

  } // end of for(;;) loop

} // end of main() function


interrupt void mainISR(void)
{
  // toggle status LED
	if(++gLEDcnt >= (uint_least32_t)(USER_ISR_FREQ_Hz / LED_BLINK_FREQ_Hz))
	{
		HAL_toggleLed(halHandle,(GPIO_Number_e)HAL_Gpio_LED2);
		gLEDcnt = 0;
	}


  // acknowledge the ADC interrupt
  HAL_acqAdcInt(halHandle,ADC_IntNumber_1);


  // convert the ADC data
  HAL_readAdcData(halHandle,&gAdcData);


  // run the controller
  CTRL_run(ctrlHandle,halHandle,&gAdcData,&gPwmData);


  // write the PWM compare values
  HAL_writePwmData(halHandle,&gPwmData);


  if(FW_getFlag_enableFw(fwHandle) == true)
    {
      FW_incCounter(fwHandle);

      if(FW_getCounter(fwHandle) > FW_getNumIsrTicksPerFwTick(fwHandle))
        {
    	  _iq refValue;
    	  _iq fbackValue;
    	  _iq output;

    	  FW_clearCounter(fwHandle);

    	  refValue = gMotorVars.VsRef;

    	  fbackValue = gMotorVars.Vs;

    	  FW_run(fwHandle, refValue, fbackValue, &output);

    	  CTRL_setId_ref_pu(ctrlHandle, output);

    	  gMotorVars.IdRef_A = _IQmpy(CTRL_getId_ref_pu(ctrlHandle), _IQ(USER_IQ_FULL_SCALE_CURRENT_A));
        }
    }
  else
    {
      CTRL_setId_ref_pu(ctrlHandle, _IQmpy(gMotorVars.IdRef_A, _IQ(1.0/USER_IQ_FULL_SCALE_CURRENT_A)));
    }

  // setup the controller
  CTRL_setup(ctrlHandle);


  return;
} // end of mainISR() function


void updateGlobalVariables_motor(CTRL_Handle handle)
{
  CTRL_Obj *obj = (CTRL_Obj *)handle;
  int32_t tmp;

  // get the speed estimate
  gMotorVars.Speed_krpm = EST_getSpeed_krpm(obj->estHandle);

  // get the real time speed reference coming out of the speed trajectory generator
  gMotorVars.SpeedTraj_krpm = _IQmpy(CTRL_getSpd_int_ref_pu(handle),EST_get_pu_to_krpm_sf(obj->estHandle));

  // get the torque estimate
  gMotorVars.Torque_Nm = USER_computeTorque_Nm(handle, gTorque_Flux_Iq_pu_to_Nm_sf, gTorque_Ls_Id_Iq_pu_to_Nm_sf);

  // when calling EST_ functions that return a float, and fpu32 is enabled, an integer is needed as a return
  // so that the compiler reads the returned value from the accumulator instead of fpu32 registers
  // get the magnetizing current
  tmp = EST_getIdRated(obj->estHandle);
  gMotorVars.MagnCurr_A = *((float_t *)&tmp);

  // get the rotor resistance
  tmp = EST_getRr_Ohm(obj->estHandle);
  gMotorVars.Rr_Ohm = *((float_t *)&tmp);

  // get the stator resistance
  tmp = EST_getRs_Ohm(obj->estHandle);
  gMotorVars.Rs_Ohm = *((float_t *)&tmp);

  // get the stator inductance in the direct coordinate direction
  tmp = EST_getLs_d_H(obj->estHandle);
  gMotorVars.Lsd_H = *((float_t *)&tmp);

  // get the stator inductance in the quadrature coordinate direction
  tmp = EST_getLs_q_H(obj->estHandle);
  gMotorVars.Lsq_H = *((float_t *)&tmp);

  // get the flux in V/Hz in floating point
  tmp = EST_getFlux_VpHz(obj->estHandle);
  gMotorVars.Flux_VpHz = *((float_t *)&tmp);

  // get the flux in Wb in fixed point
  gMotorVars.Flux_Wb = USER_computeFlux(handle, gFlux_pu_to_Wb_sf);

  // get the controller state
  gMotorVars.CtrlState = CTRL_getState(handle);

  // get the estimator state
  gMotorVars.EstState = EST_getState(obj->estHandle);

  // read Vd and Vq vectors per units
  gMotorVars.Vd = CTRL_getVd_out_pu(ctrlHandle);
  gMotorVars.Vq = CTRL_getVq_out_pu(ctrlHandle);

  // calculate vector Vs in per units
  gMotorVars.Vs = _IQsqrt(_IQmpy(gMotorVars.Vd, gMotorVars.Vd) + _IQmpy(gMotorVars.Vq, gMotorVars.Vq));

  // read Id and Iq vectors in amps
  gMotorVars.Id_A = _IQmpy(CTRL_getId_in_pu(ctrlHandle), _IQ(USER_IQ_FULL_SCALE_CURRENT_A));
  gMotorVars.Iq_A = _IQmpy(CTRL_getIq_in_pu(ctrlHandle), _IQ(USER_IQ_FULL_SCALE_CURRENT_A));

  // calculate vector Is in amps
  gMotorVars.Is_A = _IQsqrt(_IQmpy(gMotorVars.Id_A, gMotorVars.Id_A) + _IQmpy(gMotorVars.Iq_A, gMotorVars.Iq_A));

  // Get the DC buss voltage
  gMotorVars.VdcBus_kV = _IQmpy(gAdcData.dcBus,_IQ(USER_IQ_FULL_SCALE_VOLTAGE_V/1000.0));

  return;
} // end of updateGlobalVariables_motor() function


void updateKpKiGains(CTRL_Handle handle)
{
  if((gMotorVars.CtrlState == CTRL_State_OnLine) && (gMotorVars.Flag_MotorIdentified == true) && (Flag_Latch_softwareUpdate == false))
    {
      // set the kp and ki speed values from the watch window
      CTRL_setKp(handle,CTRL_Type_PID_spd,gMotorVars.Kp_spd);
      CTRL_setKi(handle,CTRL_Type_PID_spd,gMotorVars.Ki_spd);

      // set the kp and ki current values for Id and Iq from the watch window
      CTRL_setKp(handle,CTRL_Type_PID_Id,gMotorVars.Kp_Idq);
      CTRL_setKi(handle,CTRL_Type_PID_Id,gMotorVars.Ki_Idq);
      CTRL_setKp(handle,CTRL_Type_PID_Iq,gMotorVars.Kp_Idq);
      CTRL_setKi(handle,CTRL_Type_PID_Iq,gMotorVars.Ki_Idq);
	}

  return;
} // end of updateKpKiGains() function


interrupt void timer0ISR(void){
	// acknowledge the Timer 0 interrupt
	HAL_acqTimer0Int(halHandle);
	// toggle status LED
	//HAL_toggleLed(halHandle,HAL_GPIO_LED3);
	timer0_count++;
	return;
} // end of timer0ISR() function

#ifdef ecan_test

void mailbox_read(ECAN_Handle handle, int_least16_t MBXnbr){
   volatile struct MBOX *Mailbox = (&(handle->ECanaMboxes->MBOX0)) + MBXnbr;
   TestMbox1 = Mailbox->MDL.all; // = 0x9555AAAn (n is the MBX number)
   TestMbox2 = Mailbox->MDH.all; // = 0x89ABCDEF (a constant)
   TestMbox3 = Mailbox->MSGID.all;// = 0x9555AAAn (n is the MBX number)

} // MSGID of a rcv MBX is transmitted as the MDL data.

void mailbox_check(int_least32_t T1, int_least32_t T2, int_least32_t T3){
    if((T1 != T3) || ( T2 != 0x89ABCDEF))
    {
    	ErrorCount++;
    }
    else
    {
    	PassCount++;
    }
}
#endif


//@} //defgroup
// end of file


