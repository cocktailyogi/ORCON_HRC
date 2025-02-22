#include "orcon_fan_control.h"
#include "ELECHOUSE_CC1101.h"

#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define SS_PIN 5


namespace esphome {
namespace orcon_fan_control {

//ELECHOUSE_CC1101 ELECHOUSE_cc1101;

void OrconFanControl::set_airflow_level(number::Number *airflow_level) {
    airflow_level->add_on_state_callback([this](float new_airflow_level) {
        this->airflow_level = static_cast<int>(new_airflow_level);
        this->on_set_Airflow(this->airflow_level);
    });
}

void OrconFanControl::setup() {
	
	//ESP_LOGD("custom", "Setup Started");
	int gdo0 = 8;
    int gdo2 = 7;
	//ELECHOUSE_cc1101.setGDO(gdo0, gdo2); // #GDO0 8, GDO2 7
	ELECHOUSE_cc1101.setSpiPin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
    ELECHOUSE_cc1101.Init();
	
    ELECHOUSE_cc1101.setModulation(0); // set modulation mode. 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.
    ELECHOUSE_cc1101.setMHZ(868.17); // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
    ELECHOUSE_cc1101.setSyncMode(1); // Combined sync-word qualifier mode. 0 = No preamble/sync. 1 = 16 sync word bits detected. 2 = 16/16 sync word bits detected. 3 = 30/32 sync word bits detected. 4 = No preamble/sync, carrier-sense above threshold. 5 = 15/16 + carrier-sense above threshold. 6 = 16/16 + carrier-sense above threshold. 7 = 30/32 + carrier-sense above threshold.
    ELECHOUSE_cc1101.setCrc(0); // 1 = CRC calculation in TX and CRC check in RX enabled. 0 = CRC disabled for TX and RX.
    ELECHOUSE_cc1101.SpiWriteReg(CC1101_DEVIATN, 0x50); // Deviation 50.7 KHz
            //ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSCTRL0, 0x00); // 
            //ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSCTRL1, 0x08); // 
    ELECHOUSE_cc1101.setDRate(38.4); //38400 Baud
    ELECHOUSE_cc1101.setRxBW(325); //Bandwidth 325 kHz
    ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG0, 0x2E); // disable Tristate
	ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG2, 0x2E); // disable Tristate
	ELECHOUSE_cc1101.SpiWriteReg(CC1101_FIFOTHR, 0b00000111); // set Fifo Threshold
	ELECHOUSE_cc1101.SpiWriteReg(CC1101_PKTCTRL0, 0b00000010); // set Packet Mode
	ELECHOUSE_cc1101.SpiWriteReg(CC1101_PKTCTRL1, 0b00000000); // set Packet Mode
	ELECHOUSE_cc1101.SpiWriteReg(CC1101_BSCFG, 0x1C); // 
	ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b00100000); //?
    ELECHOUSE_cc1101.setFEC(0);
    ELECHOUSE_cc1101.setPRE(4);
    ELECHOUSE_cc1101.setSyncWord(0xFF, 0x00);
    ELECHOUSE_cc1101.setAppendStatus(0);
    ELECHOUSE_cc1101.setLengthConfig(2);
    ELECHOUSE_cc1101.setPA(10);
	
    this->on_set_Airflow(this->airflow_level);
	//ESP_LOGD("custom", "Setup finished");
}

void OrconFanControl::on_set_Airflow(int level) {
	//setup();
    for (int i = 0; i < 10; ++i) {
		//ESP_LOGD("custom", "on_set_Airflow mit level: %d", level);
        switch (level) {
            case 1:
                packet_template[33] = 0x95;
                packet_template[38] = 0x65;
                break;
            case 2:
                packet_template[33] = 0x65;
                packet_template[38] = 0x95;
                break;
            case 3:
                packet_template[33] = 0xA5;
                packet_template[38] = 0x55;
                break;
        }
		
		//ESP_LOGD("custom", "marcstate 0x%x", ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE));
		
        ESP_LOGD("custom", "CC1101-request try No. %d", i + 1);
		
		int NoBytesFifo = 255;
        int expectedNoOfBytes = 43;
        ELECHOUSE_cc1101.SpiWriteBurstReg(CC1101_TXFIFO, packet_template, 43);
		ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
		ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
        ELECHOUSE_cc1101.SpiStrobe(CC1101_STX); //start send
		
		unsigned long start = millis();
        while (NoBytesFifo != 0) {
			NoBytesFifo = ELECHOUSE_cc1101.SpiReadStatus(CC1101_TXBYTES) & 0b01111111;
			//ESP_LOGD("custom", "Number of Bytes in TX Fifo: %d",NoBytesFifo);
			if (millis() - start > 100) {  // Max 100ms warten
                ESP_LOGW("custom", "CC1101 send timeout!");
                return;
            }
            delay(1);
        }
		//ESP_LOGD("custom", "send done");

        ELECHOUSE_cc1101.SetRx();
        delay(40);
		
		NoBytesFifo = ELECHOUSE_cc1101.SpiReadStatus(CC1101_RXBYTES) & 0b01111111;
		
        if (NoBytesFifo >= expectedNoOfBytes) {
			int rssi = ELECHOUSE_cc1101.getRssi();
			ESP_LOGD("custom", "Got Something, RSSI = %d", rssi);
			byte RX_FIFO[expectedNoOfBytes] = {0};
			ELECHOUSE_cc1101.SpiReadBurstReg(CC1101_RXFIFO,RX_FIFO,expectedNoOfBytes);
			ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
			if (RX_FIFO[0] == 0x2C && RX_FIFO[1] == 0xCA && RX_FIFO[2] == 0xAA && RX_FIFO[3] == 0xCA) {
				ESP_LOGD("custom", "Transmission okay");
				break;
			}
			else {
				ESP_LOGD("custom", "Transmission error");
			}
			
		}
		else {
			ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
			ESP_LOGD("custom", "RX failed, not enough bytes");
		}

	delay(50);
    }
}

}  // namespace orcon_fan_control
}  // namespace esphome
