/*
 * miniscope.c
 *
 *  Created on: Jan 20, 2020
 *      Author: DBAharoni
 */

#include <cyu3system.h>
#include <cyu3os.h>
#include <cyu3dma.h>
#include <cyu3error.h>
#include <cyu3uart.h>
#include <cyu3i2c.h>
#include <cyu3types.h>
#include <cyu3gpio.h>
#include <cyu3utils.h>

#include "miniscope.h"

// ------------- Initialize globals ----
uint32_t dFrameNumber = 0;
uint32_t currentTime = 0;

CyBool_t recording = CyFalse;

CyBool_t endOfFrame = CyFalse;

CyBool_t bnoEnabled = CyFalse;

uint8_t quatBNO[8] = {0,0,0,0,0,0,0,0};

//--- EWL  plane jump constants

uint8_t ewlPlaneNumber = 0;
uint8_t ewlNumPlanes = 0;
uint8_t ewlPlaneValue[2] = {0,0};
uint8_t LEDNum = 0;


//--- LED toggle constants

// EWL Plane jumping function
// Receives the planes information from the deviceConfig file

void handleEWLPlaneJumping(void) {

	if (ewlNumPlanes > 0) {

		CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
		CyU3PI2cPreamble_t preamble;
		uint8_t dataBuff[2];

		ewlPlaneNumber = (ewlPlaneNumber + 1) % ewlNumPlanes;

		preamble.buffer[0] = EWL_ADDR; // I2C Address
		preamble.buffer[1] = 0x08; // usual reg byte
		preamble.length    = 2; //register length + 1 (for address)
		preamble.ctrlMask  = 0x0000;

		dataBuff[0] = ewlPlaneValue[ewlPlaneNumber];
		dataBuff[1] = 0x02;

		apiRetStatus = CyU3PI2cTransmitBytes (&preamble, dataBuff, 2, 0);
		if (apiRetStatus == CY_U3P_SUCCESS)
			CyU3PBusyWait (100);
		else
			CyU3PDebugPrint (2, "I2C DAC WRITE command failed\r\n");

	}
}

uint8_t getNumPlanes(void) {
	return ewlNumPlanes;
}

// LED toggle

void handleLEDDisable(void) {

	if (ewlNumPlanes > 0) {

		CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
		CyU3PI2cPreamble_t preamble;
		uint8_t dataBuff[1];

		LEDNum ^= 1;

		preamble.buffer[0] = MCU_ADDR; // I2C Address
		preamble.buffer[1] = 0x02; // usual reg byte
		preamble.length    = 2; //register length + 1 (for address)
		preamble.ctrlMask  = 0x0000;

		dataBuff[0] = 0xF0;

		apiRetStatus = CyU3PI2cTransmitBytes (&preamble, dataBuff, 1, 0);
		if (apiRetStatus == CY_U3P_SUCCESS)
			CyU3PBusyWait (100);
		else
			CyU3PDebugPrint (2, "I2C DAC WRITE command failed\r\n");

	}
}

void handleLEDEnable(void) {

	if (ewlNumPlanes > 0) {

		CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
		CyU3PI2cPreamble_t preamble;
		uint8_t dataBuff[1];

		LEDNum ^= 1;

		preamble.buffer[0] = MCU_ADDR; // I2C Address
		preamble.buffer[1] = 0x01; // usual reg byte
		preamble.length    = 2; //register length + 1 (for address)
		preamble.ctrlMask  = 0x0000;

		dataBuff[0] = 1;

		apiRetStatus = CyU3PI2cTransmitBytes (&preamble, dataBuff, 1, 0);
		if (apiRetStatus == CY_U3P_SUCCESS)
			CyU3PBusyWait (100);
		else
			CyU3PDebugPrint (2, "I2C DAC WRITE command failed\r\n");

	}
}

void handleLEDtoggle(void) {

	if (ewlNumPlanes > 0) {

		CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
		CyU3PI2cPreamble_t preamble;
		uint8_t dataBuff[1];

		LEDNum ^= 1;

		preamble.buffer[0] = DigitalPot_ADDR; // I2C Address
		preamble.buffer[1] = 0x00; // usual reg byte
		preamble.length    = 1; //register length + 1 (for address)
		preamble.ctrlMask  = 0x0000;

		dataBuff[0] = ewlPlaneValue[ewlPlaneNumber];


		apiRetStatus = CyU3PI2cTransmitBytes (&preamble, dataBuff, 1, 0);
		if (apiRetStatus == CY_U3P_SUCCESS)
			CyU3PBusyWait (100);
		else
			CyU3PDebugPrint (2, "I2C DAC WRITE command failed\r\n");

	}
}


/**
 *  Initializes our I2C packet ringbuffer using a preallocated structure.
 *  @return 0 on success.
 */
int i2c_packet_queue_init (I2CPacketQueue *pq)
{
    pq->pendingCount = 0;
    pq->idxRD = 0;
    pq->idxWR = 0;
    pq->curPacketParts = I2C_PACKET_PART_NONE;

    if (CyU3PMutexCreate (&pq->lock, CYU3P_NO_INHERIT) != CY_U3P_SUCCESS)
        return -1;
    return 0;
}

void i2c_packet_queue_free (I2CPacketQueue *pq)
{
    CyU3PMutexPut(&pq->lock);
    CyU3PMutexDestroy(&pq->lock);
}

void i2c_packet_queue_lock (I2CPacketQueue *pq)
{
    CyU3PMutexGet (&(pq->lock), CYU3P_WAIT_FOREVER);
}

void i2c_packet_queue_unlock (I2CPacketQueue *pq)
{
    CyU3PMutexPut (&(pq->lock));
}

void i2c_packet_queue_wrnext_if_complete (I2CPacketQueue *pq, I2CPacketPartFlags flag_added)
{
    pq->curPacketParts = pq->curPacketParts | flag_added;
    if (pq->curPacketParts != (I2C_PACKET_PART_HEAD | I2C_PACKET_PART_BODY | I2C_PACKET_PART_TAIL))
        return; /* we don't have a complete packet yet */

    /* advance in the write buffer */
    pq->idxWR = (pq->idxWR + 1) % I2C_PACKET_BUFFER_SIZE;
    pq->pendingCount++;

    /* we have no parts registered yet */
    pq->curPacketParts = I2C_PACKET_PART_NONE;
}

//----------------------------------

void I2CProcessAndSendPendingPacket (I2CPacketQueue *pq)
{
	CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
	CyU3PI2cPreamble_t preamble;
	uint8_t dataBuff[9];
	uint8_t packetLength;
	int i;

	if (pq->pendingCount == 0)
		return;
	i2c_packet_queue_lock (pq);

	while (pq->pendingCount > 0) {
		if (pq->buffer[pq->idxRD][0] == 0xFE) {
			if (pq->buffer[pq->idxRD][1] == 2) {
				// User for configuring DAQ and not as I2C pass through
				handleDAQConfigCommand(pq->buffer[pq->idxRD][2]);
			}
			else if (pq->buffer[pq->idxRD][1] > 2 && pq->buffer[pq->idxRD][2] == 0x01){
				// Used for EWL focal plane jumping settings
				ewlPlaneNumber = 0;
				ewlNumPlanes = pq->buffer[pq->idxRD][1] - 2;
				for (uint8_t i = 0; i < ewlNumPlanes; i++) {
					ewlPlaneValue[i] = pq->buffer[pq->idxRD][3 + i];
				}
			}
		} else {
			if (pq->buffer[pq->idxRD][0] & 0x01) {
				// This denotes a full packet of 6 bytes.
				packetLength = 6; // Number of bytes in packet
				preamble.buffer[0] = pq->buffer[pq->idxRD][0]&0xFE; // I2C Address
				preamble.buffer[1] = pq->buffer[pq->idxRD][1]; // usual reg byte
				preamble.length    = 2; //register length + 1 (for address)
				preamble.ctrlMask  = 0x0000;

				for (i=0; i< (packetLength-2);i++){
					dataBuff[i] = pq->buffer[pq->idxRD][i+2];
				}
			}
			else {
				// less than 6 bytes in packet
				packetLength = pq->buffer[pq->idxRD][1]; // Number of bytes in packet
				preamble.buffer[0] = pq->buffer[pq->idxRD][0]; // I2C Address
				preamble.buffer[1] = pq->buffer[pq->idxRD][2]; // usual reg byte
				preamble.length    = 2; //register length + 1 (for address)
				preamble.ctrlMask  = 0x0000;

				for (i=0; i< (packetLength-2);i++){
					dataBuff[i] = pq->buffer[pq->idxRD][i+3];
				}
			}

			apiRetStatus = CyU3PI2cTransmitBytes (&preamble, dataBuff, packetLength - 2, 0);
			if (apiRetStatus == CY_U3P_SUCCESS)
				CyU3PBusyWait (100);
			else
				CyU3PDebugPrint (2, "I2C DAC WRITE command failed\r\n");
		}
		pq->idxRD = (pq->idxRD + 1) % I2C_PACKET_BUFFER_SIZE;
		pq->pendingCount--;
	}

	/* release lock on the queue */
	i2c_packet_queue_unlock (pq);
}

void handleDAQConfigCommand(uint8_t command) {
	switch(command) {
		case(0x00):
			bnoEnabled = CyTrue;
			break;
		default:
			break;
	}
}
CyU3PReturnStatus_t readBNO(void) {
	CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
	CyU3PI2cPreamble_t preamble;


	preamble.buffer[0] = BNO055_ADDR & 0xFE; /*  Mask out the transfer type bit. */
	preamble.buffer[1] = 0x20; // We can use 0x00 here for testing I2C communication stability
	preamble.buffer[2] = BNO055_ADDR | 0x01;
	preamble.length    = 3;
	preamble.ctrlMask  = 0x0002;                                /*  Send start bit after third byte of preamble. */

	apiRetStatus = CyU3PI2cReceiveBytes (&preamble, (uint8_t *)quatBNO, 8, 0);

	if (apiRetStatus == CY_U3P_SUCCESS) {
	        CyU3PBusyWait (200); //in us
	}

	return apiRetStatus;
}
