/*
  (C) Andreas Vogel andreas@wellenvogel.de
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef _GWSPISENSOR_H
#define _GWSPISENSOR_H
#include <driver/spi_master.h>
#include "GwSensor.h"
#include <memory>

class SPIBus{
    spi_host_device_t hd;
    bool initialized=false;
    public:
    SPIBus(spi_host_device_t h):hd(h){}
    bool init(GwLog*logger,int mosi=-1,int miso=-1,int clck=-1){
        spi_bus_config_t buscfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = clck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0
        };   
        esp_err_t res=spi_bus_initialize(hd,&buscfg,0);
        if (res == ESP_OK){
            LOG_DEBUG(GwLog::LOG,"initialzed SPI bus %d,mosi=%d,miso=%d,clock=%d",
                (int)hd,mosi,miso,clck);
            initialized=true;
        }
        else{
            LOG_DEBUG(GwLog::ERROR,"unable to initialize SPI bus %d,mosi=%d,miso=%d,clock=%d, error=%d",
            (int)hd,mosi,miso,clck,(int)res);
        }
        return initialized;
    }
    spi_host_device_t host() const { return hd;} 
};

using BusType=SPIBus;

class SSIDevice{
    spi_device_handle_t spi;
    spi_host_device_t host;
    bool initialized=false;
    int bits=12;
    public:
    SSIDevice(const SPIBus *bus):host(bus->host()){}
    bool init(GwLog*logger,int clock,int cs=-1,int bits=12){
        this->bits=bits;
       spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 2,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans =0,
        .clock_speed_hz = clock, 
        .input_delay_ns =0,
        .spics_io_num = cs,             //CS pin
        .queue_size = 1 //see https://github.com/espressif/esp-idf/issues/9450
        };
        esp_err_t res=spi_bus_add_device(host,&devcfg,&spi);
        if (res == ESP_OK){
            LOG_DEBUG(GwLog::LOG,"added SSI device to bus %d, cs=%d, clock=%d",
                (int)host,cs,clock);
            initialized=true;
        }
        else{
            LOG_DEBUG(GwLog::ERROR,"unable to add SSI device to bus %d, cs=%d, clock=%d, error=%d",
                (int)host,cs,clock,(int) res);
        }
        return initialized;
    }
    bool isInitialized() const { return initialized;}
    spi_device_handle_t & device(){return spi;}

};


class SSISensor : public SensorBase<BusType>{
    std::unique_ptr<SSIDevice> device;
    protected:
    int bits=12;
    int mask=0xffff;
    int cs=-1;
    int clock=0;
    bool act=false;
    virtual bool initSSI(GwLog*logger,const SPIBus *bus,
        int clock,int cs, int bits){
            mask= (1 << bits)-1;
            device.reset(new SSIDevice(bus));
            return device->init(logger,clock,cs,bits);
        }
        esp_err_t readData(uint32_t &res)
        {
            struct spi_transaction_t ta = {
                .flags = SPI_TRANS_USE_RXDATA,
                .cmd = 0,
                .addr = 0,
                .length = bits+1,
                .rxlength = 0};
            esp_err_t ret = spi_device_queue_trans(device->device(), &ta, portMAX_DELAY);
            if (ret != ESP_OK) return ret;
            struct spi_transaction_t *rs = NULL;
            ret = spi_device_get_trans_result(device->device(), &rs, portMAX_DELAY);
            if (ret != ESP_OK) return ret;
            if (rs == NULL) return ESP_ERR_INVALID_RESPONSE;
            res=SPI_SWAP_DATA_RX(*(uint32_t*)rs->rx_data,bits+1);
            res&=mask;
            return ESP_OK;
        }

    public:
    using SensorBase<BusType>::SensorBase;
    virtual bool isActive(){return act;};
    virtual bool initDevice(GwApi *api,BusType *bus){
        return initSSI(api->getLogger(),bus, clock,cs,bits);
    };
    
};
using SpiSensorList=SensorList<BusType>;
#define GWSPIHOST1 SPI2_HOST
#define GWSPIHOST2 SPI3_HOST
#endif