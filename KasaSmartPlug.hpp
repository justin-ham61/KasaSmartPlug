/*
Author: Kris Jearakul
Copyright (c) 2022 Kris Jearakul
https://github.com/kj831ca/KasaSmartPlug

This library allows the ESP32 board to scan the TP-Link smart plug devices on your local network.
It also allow you to control the TP-Link Smart Plug on your local network.

Support Devices Model: HS103 and HS200 because I only have these model to test.

Credit:
Thanks to the below infomation link.
https://www.softscheck.com/en/reverse-engineering-tp-link-hs110/

This program is free software: you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.
If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef KASA_SMART_PLUG_DOT_HPP
#define KASA_SMART_PLUG_DOT_HPP

#include <WiFi.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include <ArduinoJson.h>

#define KASA_ENCRYPTED_KEY 171
#define MAX_PLUG_ALLOW 10

class KASADevice{
    protected:
    int sock;
    struct sockaddr_in dest_addr;
    static SemaphoreHandle_t mutex;
    StaticJsonDocument<1024> doc;
    
    bool OpenSock(){
        int err;
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

        fd_set fdset;
        struct timeval tv;
        int arg;

        if(sock < 0){
            Serial.println("Error: Failed to open a socket for Device");
            return false;
        }

        arg = fcntl(sock, F_GETFL, NULL);
        arg |= O_NONBLOCK;

        fcntl(sock, F_SETFL, O_NONBLOCK);

        err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if(err < 0){
            do{
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                FD_ZERO(&fdset);
                FD_SET(sock, &fdset);

                err = select(sock + 1, NULL, &fdset, NULL, &tv);
                if(err < 0 && errno != EINTR){
                    Serial.println("Unable to open socket");
                    break;
                }
                if(err == 1){
                    int so_error = 0;
                    socklen_t len = sizeof so_error;
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
                    if (so_error == 0){
                        fcntl(sock, F_SETFL, arg);
                        return true;
                    } else {
                        break;
                    }
                }
            } while(1);
        }
        Serial.println("Error: Failed to open socket for Device");
        CloseSock();
        return false;
    }

    void CloseSock(){
        if(sock != -1){
            shutdown(sock, 0);
            close(sock);
            sock = -1;
        }
    }

    int Query(const char*cmd, char *buffer, int bufferLength, long timeout);

    public:
    char alias[32];
    char ip_address[32];
    char model[15];
    int state;
    int err_code;


    void SendCommand(const char *cmd);

    void UpdateIPAddress(const char *ip){
        strcpy(ip_address, ip);
        sock = -1;
        dest_addr.sin_addr.s_addr = inet_addr(ip_address);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(9999);
    }

    KASADevice(const char *name , const char *ip){
        strcpy(alias, name);
        UpdateIPAddress(ip);
        err_code = 0;
        xSemaphoreGive(mutex);
    }

    virtual ~KASADevice(){}

    virtual const char* getType() {
        return "KASADevice";
    }
};

class KASASmartBulb: public KASADevice{
    public:
    int brightness;
    int temp;
    int GetDeviceInfo();

    void turnOn();
    void turnOff();
    void toggle();
    void setBrightness();
    void setTemperature();

    KASASmartBulb(const char *name, const char *ip, int brightness, int temp)
        :KASADevice(name, ip), brightness(brightness), temp(temp){}

    virtual const char* getType() override{
        return "KASASmartBulb";
    }
};

//Smart Plug Class
class KASASmartPlug: public KASADevice{
    protected:
    void DebugBufferPrint(char *data, int length);

    public:

    int QueryInfo();
    void SetRelayState(uint8_t state);

    //Constructor for KASA Smart Plug
    KASASmartPlug(const char *name, const char *ip)
        :KASADevice(name, ip){}

    virtual const char* getType() override {
        return "KASASmartPlug";
    }
};

class KASAUtil {

    private:
    //Array of pluts initialized to the size MAX_PLUG_ALLOW
    KASADevice *ptr_plugs[MAX_PLUG_ALLOW];
    void closeSock(int sock);
    int IsContainPlug(const char *name);
    int IsStartWith(const char *prefix, const char *model)
    {
        return strncmp(prefix, model, strlen(prefix)) == 0;
    }
    int deviceFound;

public:
    static const char *get_kasa_info;
    static const char *relay_on;
    static const char *relay_off;
    static const char *light_on;
    static const char *light_off;
    static const char* set_brightness;
    static const char* set_temperature;

    int ScanDevices(int timeoutMs = 1000); // Wait at least xxx ms after received UDP packages..
    static uint16_t Encrypt(const char *data, int length, uint8_t addLengthByte, char *encryped_data);
    static uint16_t Decrypt(char *data, int length, char *decryped_data, int startIndex);
    void CreateAndDeliver(const char *ip, const int req, const char *type);
    KASADevice *GetSmartPlug(const char *alias_name);
    KASADevice *GetSmartPlugByIndex(int index);
    KASAUtil();
};



#endif