/*
 * miniscope.h
 *
 *  Created on: Jan 20, 2020
 *      Author: DBAharoni
 */

#ifndef MINISCOPE_H_
#define MINISCOPE_H_

#include "definitions.h"
#include <cyu3types.h>

// ------- Globals ---------------------
#define BNO055_ADDR            0b01010000
#define MCU_ADDR			   0b00100000
#define DigitalPot_ADDR		   0b01011000
#define EWL_ADDR			   0b11101110
#define I2C_PACKET_BUFFER_SIZE 64

// ------ 2 color switching constants

#define LED0_MCU_ENABLE 0x01
#define LED0_DP_ENABLE 0x00

#define LED1_MCU_ENABLE 0x02
#define LED1_DP_ENABLE 0x01

/**
 * A packet is assembled from bytes coming via various UVC camera controls
 * (Contrast, Gamma, Sharpness) and is only complete once we received
 * all parts (those parts may arrive in random order even if they were
 * sent in the right order)
 * We use these flags to keep track of the currently-assembled packet
 * in memory.
 */
typedef enum
{
    I2C_PACKET_PART_NONE = 0,
    I2C_PACKET_PART_HEAD = 1 << 0,
    I2C_PACKET_PART_BODY = 1 << 1,
    I2C_PACKET_PART_TAIL = 1 << 2
} I2CPacketPartFlags;

/**
 * I2C packet ringbuffer structure
 */
typedef struct
{
    uint8_t buffer[I2C_PACKET_BUFFER_SIZE][6]; /* circular buffer */
    uint8_t pendingCount;
    uint8_t idxRD;
    uint8_t idxWR;
    I2CPacketPartFlags curPacketParts;
    CyU3PMutex lock;
} I2CPacketQueue;

/**
 * Structure to make the plane selection more clear
 * Each plane has an EWL plane value,LED selection and brightness value
 */
/*
typedef struct
{
	uint8_t EWL_value;
	uint8_t LED_value;
	uint8_t LED_enable;

} planeSelect;
*/
extern int i2c_packet_queue_init (I2CPacketQueue *pq);
extern void i2c_packet_queue_free (I2CPacketQueue *pq);

extern void i2c_packet_queue_lock (I2CPacketQueue *pq);
extern void i2c_packet_queue_unlock (I2CPacketQueue *pq);
extern void i2c_packet_queue_wrnext_if_complete (I2CPacketQueue *pq,
                                                 I2CPacketPartFlags flag_added);

extern CyBool_t recording;

extern CyBool_t endOfFrame;

extern CyBool_t bnoEnabled;

// For BNO head orientation sensor
extern uint8_t quatBNO[8];

// Tracking frame number and timing
extern uint32_t dFrameNumber;
extern uint32_t currentTime;
//----------------------------------

extern uint8_t PlaneValue[4];

extern uint8_t MCUEnable[2];
extern uint8_t DPEnable[2];

// For EWL focal plane jumping
extern uint8_t ewlPlaneNumber;
extern uint8_t ewlNumPlanes;
extern uint8_t ewlPlaneValue[2];

// For LED toggling

extern uint8_t LEDEnables[2];
extern uint8_t LEDValues[2];
extern uint8_t LEDNum;
extern uint8_t LEDNumPlanes;
extern uint8_t CurrentPlane;
extern uint8_t PreviousPlane;

// Handles the processing and sending of generic I2C packets that have built up since previous End of Frame event
extern void I2CProcessAndSendPendingPacket (I2CPacketQueue *pq);
extern void handleDAQConfigCommand (uint8_t);
extern void handleEWLPlaneJumping(void);
extern void handleLEDtoggle(void);
extern void handleLEDEnable(void);
extern void handleLEDDisable(void);
extern uint8_t getNumPlanes(void);

extern CyU3PReturnStatus_t readBNO(void);

extern CyU3PReturnStatus_t readMCUPID_VID(void);

#endif /* MINISCOPE_H_ */
