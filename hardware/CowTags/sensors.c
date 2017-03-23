/*
 * i2c.c
 *
 *  Created on: Nov 4, 2016
 *      Author: annik
 */


/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Types.h>
#include <xdc/runtime/Timestamp.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h> //i2c


#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Header files */
#include <ti/drivers/PIN.h>
#include <ti/drivers/I2C.h> //i2c
#include <ti/drivers/UART.h> //i2c

/* Example/Board Header files */
#include "Board.h"
#include <debug.h>
#include <stdint.h>
#include <assert.h>

#include <sensors.h>
#include <IIC.h>

PIN_Config BoardGpioInitialTable[] = {
//		Board_I2C0_SDA0 | PIN_GPIO_LOW,
//		Board_I2C0_SCL0	| PIN_GPIO_HIGH,
		PIN_TERMINATE
};

PIN_State pinState;

/**constants*/
#define TASKSTACKSIZE		8192	//i2c

/**structures*/
Task_Struct task0Struct;
Char task0Stack[TASKSTACKSIZE];

/*function definition */
void Sensors_init(void){
	//PIN_init(BoardGpioInitialTable);
	Task_Params taskParams;

	Task_Params_init(&taskParams);
	taskParams.stackSize = TASKSTACKSIZE;
	taskParams.stack = &task0Stack;
	Task_construct(&task0Struct, (Task_FuncPtr)testSensors, &taskParams, NULL);
}


void getAcceleration(struct sampleData *sampleData){
	//if(verbose_sensors)System_printf("\n\nwhoamI: 0x%x \n", readI2CRegister(Board_LIS3DH_ADDR, 15)); //should read 0x33
	//System_flush();

	unsigned int	i;

	struct sampleData accelerationData[30];

    writeI2CRegister(Board_LIS3DH_ADDR, LIS3DH_REG_CTRL1, 0x27);    //all axes , HR/normal mode, 10Hz
    writeI2CRegister(Board_LIS3DH_ADDR, LIS3DH_REG_CTRL4, 0x0C);	//high res and BDU and self test off +/-2g
    writeI2CRegister(Board_LIS3DH_ADDR, LIS3DH_REG_CTRL3, 0x10);    //DRDY on INT1
    //writeI2CRegister(Board_LIS3DH_ADDR, LIS3DH_REG_TEMPCFG, 0x80);    //enable adcs
    //writeI2C(Board_LIS3DH_ADDR, LIS3DH_REG_OUT_X_L | 0x80);    //enable auto increment

    //polling status register to check for new set of data
    for(i = 0 ; i < 30 ;){
    	if( (readI2CRegister(Board_LIS3DH_ADDR,0x27) & 0x8) >> 3 == 1 ){
    		if( (readI2CRegister(Board_LIS3DH_ADDR,0x27) >> 7) == 1 ){
    			accelerationData[i].accelerometerData.x = readI2CRegister(Board_LIS3DH_ADDR,0x28) | (readI2CRegister(Board_LIS3DH_ADDR,0x29) << 8);
    			accelerationData[i].accelerometerData.y = readI2CRegister(Board_LIS3DH_ADDR,0x2A) | (readI2CRegister(Board_LIS3DH_ADDR,0x2B) << 8) ;
    			accelerationData[i].accelerometerData.z = readI2CRegister(Board_LIS3DH_ADDR,0x2C) | (readI2CRegister(Board_LIS3DH_ADDR,0x2D) << 8) ;

    			sampleData->accelerometerData.x = accelerationData[i].accelerometerData.x;
    			sampleData->accelerometerData.y = accelerationData[i].accelerometerData.y;
    			sampleData->accelerometerData.z = accelerationData[i].accelerometerData.z;

    			if(verbose_sensors) System_printf("x:%i y:%i z:%i\n", 	sampleData->accelerometerData.x ,
    																	sampleData->accelerometerData.y,
																		sampleData->accelerometerData.z);
    			System_flush();
    			i++;
    		}
    	}
    }

    for(i = 0 ; i < 30 ;i++){
    	System_printf("x:%3d y:%3d z:%3d\n", 	accelerationData[i].accelerometerData.x ,
    										accelerationData[i].accelerometerData.y,
											accelerationData[i].accelerometerData.z);
    }


    System_flush();

    return;
}

void getTemp(struct sampleData *sampleData){
	uint16_t temp_obj;
	uint16_t temp_amb;

	temp_obj =  readI2CWord100kHz(Board_MIKROE1362_ADDR,0x07)*0.02 - 273.15;
	sampleData->tempData.temp_l = temp_obj & 0xFF;
	sampleData->tempData.temp_h = temp_obj >> 8;

	temp_amb =  readI2CWord100kHz(Board_MIKROE1362_ADDR,0x06)*0.02 - 273.15;
	sampleData->heartRateData.temp_l = temp_amb & 0xFF;
	sampleData->heartRateData.temp_h = temp_amb >> 8;

	if(verbose_sensors){
		System_printf("temp_obj %d\n", temp_obj);
	}

	return;
}

void getTempNoPtr(){
	uint16_t temp_obj;
	uint16_t temp_amb;


	temp_obj =  readI2CWord100kHz(Board_MIKROE1362_ADDR,0x07)*0.02 - 273.15;
	//sampleData->tempData.temp_l = temp_obj & 0xFF;
	//sampleData->tempData.temp_h = temp_obj >> 8;


	temp_amb =  readI2CWord100kHz(Board_MIKROE1362_ADDR,0x06)*0.02 - 273.15;
	//sampleData->heartRateData.temp_l = temp_amb & 0xFF;
	//sampleData->heartRateData.temp_h = temp_amb >> 8;

	if(verbose_sensors){
		System_printf("temp_obj %d\n", temp_obj);
		System_printf("temp_obj %d\n", temp_amb);
		System_flush();
	}

	return;
}

void getHeartRate(struct sampleData *sampleData){
	System_printf("first step\n");
	System_flush();

	int i = 0;
	int j = 0;
	uint16_t numValues = 250;
	uint16_t HRData[250];
	uint16_t RedData[250];
	int Derivative[250];
	uint32_t start_timestamp;
	uint32_t end_timestamp;
	uint32_t start_clock;
	uint32_t end_clock;
	int heartRate;

	System_printf("Getting heart rate..\n");
	System_flush();

	Types_FreqHz frequency;

	//check if device is connected
	if(readI2CRegister(Board_MAX30100_ADDR,0xFF) != 0x11){
		System_printf("Hardware is not the MAX30100 : 0x%x\n",readI2CRegister(Board_MAX30100_ADDR,0xFF));
		System_flush();
	}

	writeI2CRegister(Board_MAX30100_ADDR, MAX30100_REG_MODE_CONFIGURATION, 0x02);	//enable HR only

	uint8_t previous = readI2CRegister(Board_MAX30100_ADDR, MAX30100_REG_SPO2_CONFIGURATION);
	writeI2CRegister(Board_MAX30100_ADDR,
					 MAX30100_REG_SPO2_CONFIGURATION,
					 previous & 0xA0 | 0x47);	//SPO2 cofig reg

	writeI2CRegister(Board_MAX30100_ADDR,
					 MAX30100_REG_LED_CONFIGURATION,
					 0x0f);	//set LED 50mA

	previous = readI2CRegister(Board_MAX30100_ADDR, MAX30100_REG_INTERRUPT_CONFIGURATION);
	writeI2CRegister(Board_MAX30100_ADDR,
				 	 MAX30100_REG_INTERRUPT_CONFIGURATION,
					 previous & 0x0A | 0xA0);	//turn on interrupts

	//start_clock = Clock_getTicks();
	//end_clock = Clock_getTicks();

	uint32_t tempval;

	while(i < numValues){
	//while((end_clock-start_clock)/100000 < 5){

		//clear FIFO PTR
		writeI2CRegister(Board_MAX30100_ADDR, MAX30100_REG_FIFO_WRITE_POINTER, 0x00);
		writeI2CRegister(Board_MAX30100_ADDR, MAX30100_REG_FIFO_READ_POINTER, 0x00);

		//check if hr data is ready
		tempval = readI2CRegister(Board_MAX30100_ADDR, MAX30100_REG_INTERRUPT_STATUS);

		while(!tempval){
			tempval = readI2CRegister(Board_MAX30100_ADDR, MAX30100_REG_INTERRUPT_STATUS);
			tempval &= MAX30100_HRDATA_READY;
			//System_printf(".");
			//System_flush();
			if(tempval &= MAX30100_DATA_FULL){
				System_printf("IM FULL\n");
				System_flush();
			}
		}


		//fifo data is 16 bits so 4 reads is needed
		//first 16 bits is IR data, in our case, HR data
		HRData[i] = readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA)<<8 + readI2CRegister(Board_MAX30100_ADDR, MAX30100_REG_FIFO_DATA);

		//filter out 0 measurements
		/*while(HRData[i] == 0){
			HRData[i] = readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA)<<8 + readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA);
		}

		if(HRData[i] < 40000 || HRData[i] > 60000){
			if(i != 0)
			HRData[i] = HRData[i-1];
		}*/

		//next 16 bits is useless data red led data
		RedData[i] = readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA)<<8 + readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA);

		if(i!=0) Derivative[i] = (int)(HRData[i] - HRData[i-1]);

		//Timestamp_getFreq(&frequency);

		//if(i == 0){
		//	start_timestamp = Timestamp_get32();
		//}

		//Task_sleep(10000 / Clock_tickPeriod);						//adding this to extend measurement to 10 seconds
		//Task_sleep(100000 / Clock_tickPeriod);						//adding this to extend measurement to 10 seconds


		//end_timestamp = Timestamp_get32();
		//end_clock = Clock_getTicks();
		//System_flush();
		i++;
	}

	//split data into 8 bit low and high
		sampleData->heartRateData.rate_l = HRData[0] & 0xFF;
		sampleData->heartRateData.rate_h = HRData[0] >> 8;

		if(!verbose_sensors){
		for(j = 0 ; j < i ; j++){
			System_printf("HR %5d",HRData[j]);
			System_printf("       RED %5d",RedData[j]);
			System_printf("       DER %5d\n" , Derivative[j]);
		}

		System_printf("Timestamp diff: %d\n" , end_timestamp-start_timestamp);
		System_printf("Frequency %d\n", frequency.hi << 8 | frequency.lo);
		System_printf("Time elapsed: %d\n" , (end_timestamp-start_timestamp) / (frequency.hi << 8 | frequency.lo));

		System_printf("Clock diff: %d\n" , end_clock-start_clock);
		System_printf("Time elapsed: %d\n" , (end_clock-start_clock)/100000);

		System_flush();
	}

	return;
}

//This function assumes the setup for the chip has been done already
//struct MAX30100Packet getMAX30100Packet{
//	struct MAX30100Packet MAX30100Packet;
//
//	Max30100Packet.hrHigh = readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA);
//	Max30100Packet.hrLow = readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA);
//	Max30100Packet.redHigh = readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA);
//	Max30100Packet.redLow = readI2CRegister(Board_MAX30100_ADDR,MAX30100_REG_FIFO_DATA);
//
//	return MAX30100Packet;
//};

void getTimestamp(struct sampleData *sampleData){
	sampleData->timestamp = Timestamp_get32();
}

void makeSensorPacket(struct sampleData *sampleData){

	getAcceleration(sampleData);

//	getTemp(sampleData);

//	getHeartRate(sampleData);

	//getTimestamp(sampleData);

	/*System_printf(							"TemperatureCowData = %i ,"
											"AmbientTemperatureData = %i ,"
											"InfraredData = %i\n"
											"AccelerometerData= x=%i, y=%i, z=%i\n"
											"Timestamp = %i\n",
											sampleData->tempData.temp_h << 8 | sampleData->tempData.temp_l,
											sampleData->heartRateData.temp_h << 8 | sampleData->heartRateData.temp_l,
											sampleData->heartRateData.rate_h << 8 | sampleData->heartRateData.rate_l,
											sampleData->accelerometerData.x,
											sampleData->accelerometerData.y,
											sampleData->accelerometerData.z,
											sampleData->timestamp);
*/
	System_flush();
}

void testSensors(){
	struct sampleData sampleData;

	while(1){
		makeSensorPacket(&sampleData);
	}
	System_printf("Tests done\n");
	System_flush();


//	getHeartRate(&sampleData);
}
