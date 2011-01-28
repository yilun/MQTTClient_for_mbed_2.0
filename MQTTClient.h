/**
 *A simple MQTT client for mbed, version 2.0
 *By Yilun FAN, @CEIT, @JAN 2011
 *
 */
#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "mbed.h"
#include "TCPSocket.h"

#define MQTTCONNECT 1<<4
#define MQTTPUBLISH 3<<4
#define MQTTSUBSCRIBE 8<<4

#define MAX_PACKET_SIZE 128
#define KEEPALIVE 15000

class MQTTClient 
{
public:
    MQTTClient(IpAddr server, int port, void (*callback)(char*, char*));
    ~MQTTClient();
    int connect(char *);
    void disconnect();
    int publish(char *, char *);
    int subscribe(char *);
    void live();
	
private:
    int open_session(char* id);
    void read_open_session();
    int send_data(const char* msg, int size);
    void read_data();
    
    char* clientId;
    Timer timer;
    IpAddr serverIp;
    int port;
    bool connected;
    bool sessionOpened;
    
    void onTCPSocketEvent(TCPSocketEvent e);
    TCPSocket* pTCPSocket;
    Host host;
    
    int lastActivity;
    void (*callback_server)(char*, char*);
};

#endif