#include "MQTTClient.h"

MQTTClient::MQTTClient(IpAddr server, int port, void (*callback)(char*, char*))
{
    this->port = port;
    callback_server = callback;
    serverIp = server;
    connected = false;
    sessionOpened = false;
    timer.start();
}

MQTTClient::~MQTTClient()
{
}

int MQTTClient::send_data(const char* msg, int size)
{
    int transLen = pTCPSocket->send(msg, size);
    
    /*Check send length matches the message length*/
    if(transLen != size){
        for(int i = 0; i < size; i++) {
            printf("%x, ", msg[i]);
        }
        printf("Error on send.\r\n");
        return -1;
    }
    return 1;
}

int MQTTClient::open_session(char* id)
{
    /*variable header*/
    char var_header[] = {0x00,0x06,0x4d,0x51,0x49,0x73,0x64,0x70,0x03,0x02,0x00,KEEPALIVE/500,0x00,strlen(id)};
    
    /*fixed header: 2 bytes, big endian*/
    char fixed_header[] = {MQTTCONNECT,12+strlen(id)+2};
	
    char packet[sizeof(fixed_header) + sizeof(var_header) + sizeof(id)];
    
    memset(packet,0,sizeof(packet));
    memcpy(packet,fixed_header,sizeof(fixed_header));
    memcpy(packet+sizeof(fixed_header),var_header,sizeof(var_header));
    memcpy(packet+sizeof(fixed_header)+sizeof(var_header), id, strlen(id));
    
    /*
	 for(int i = 0; i < sizeof(packet); i++) {
	 printf("%x, ", packet[i]);
	 }
	 printf("\r\n");*/
    
    if(!send_data(packet, sizeof(packet)))
    {
        return -1;
    }
    return 1;
}

int MQTTClient::connect(char* id)
{
    clientId = id;
    
    /*Initial TCP socket*/
    pTCPSocket = new TCPSocket;
    pTCPSocket->setOnEvent(this, &MQTTClient::onTCPSocketEvent);
    
    host.setPort(1883);
    host.setIp(serverIp);
    host.setName("localhost");
    
    /*Trying to connect to host*/
    printf("Trying to connect to host..\r\n\r\n");
    TCPSocketErr err = pTCPSocket->connect(host);
	
    Net::poll();   
    if(err)
    {
        printf("Error connecting to host [%d]\r\n", (int) err);
        return -1;
    } 
    printf("Connect to host sucessed..\r\n\r\n");    
	
    /*Wait TCP connection with server to be established*/
    int i = 0;
    while(!connected)
    {
		Net::poll();
		wait(1);
		i++;
		printf("Wait for connections %d..\r\n", i);
		if(i == 35)
		{//If wait too long, give up.
            return -1;
		}
    }
    
    /*Send open session message to server*/
    open_session(id);
    
    /*Wait server notice of open sesion*/
    while(!sessionOpened)
    {
		Net::poll();
		wait(1);
		if(!connected){
            break;
		}
		printf("Wait for session..\r\n");
    }
    if(!connected)
    {
        return -3;
    }
    lastActivity = timer.read_ms();
    return 1;
}

int MQTTClient::publish(char* pub_topic, char* msg) 
{
    uint8_t var_header_pub[strlen(pub_topic)+3];
    strcpy((char *)&var_header_pub[2], pub_topic);
    var_header_pub[0] = 0;
    var_header_pub[1] = strlen(pub_topic);
    var_header_pub[sizeof(var_header_pub)-1] = 0;
    
    uint8_t fixed_header_pub[] = {MQTTPUBLISH,sizeof(var_header_pub)+strlen(msg)};
    
    uint8_t packet_pub[sizeof(fixed_header_pub)+sizeof(var_header_pub)+strlen(msg)];
    memset(packet_pub,0,sizeof(packet_pub));
    memcpy(packet_pub,fixed_header_pub,sizeof(fixed_header_pub));
    memcpy(packet_pub+sizeof(fixed_header_pub),var_header_pub,sizeof(var_header_pub));
    memcpy(packet_pub+sizeof(fixed_header_pub)+sizeof(var_header_pub),msg,strlen(msg));
    
    if(!send_data((char*)packet_pub, sizeof(packet_pub)))
    {
        return -1;
    }
    return 1;
}


void MQTTClient::disconnect() 
{
    uint8_t packet_224[] = {2,2,4};
    
    send_data((char*)packet_224, sizeof(packet_224));
    send_data(0, 1);
    
    connected = false;
}

void MQTTClient::read_data()
{
    char buffer[1024];
    int len = 0, readLen;
	
    while((readLen = pTCPSocket->recv(buffer, 1024)) != 0){
        len += readLen;
    }
    
    buffer[len] = '\0';
    
    /*
	 printf("Read length: %d %d\r\n", len, readLen);
	 
	 for(int i = 0; i < len; i++) {
	 printf("%x\r\n", buffer[i],buffer[i]);
	 }
	 printf("\r\n");
	 */
    char type = buffer[0]>>4;
    if (type == 3) { // PUBLISH
		if (callback_server) {
			uint8_t tl = (buffer[2]<<3)+buffer[3];
			char topic[tl+1];
			for (int i=0;i<tl;i++) {
				topic[i] = buffer[4+i];
			}
			topic[tl] = 0;
			// ignore msgID - only support QoS 0 subs
			char *payload = buffer+4+tl;
			callback_server(topic,(char*)payload);
		}
    } else if (type == 12) { // PINGREG -- Ask for alive
        char packet_208[] = {0xd0, 0x00};
        send_data((char*)packet_208, 2);
        lastActivity = timer.read_ms();
    }
}

void MQTTClient::read_open_session()
{
    char buffer[32];
    int len = 0, readLen;
	
    while((readLen = pTCPSocket->recv(buffer, 32)) != 0)
    {
        len += readLen;
    }
	
    if(len == 4 && buffer[3] == 0)
    {
        printf("Session opened\r\n");
        sessionOpened = true;
    }
}

int MQTTClient::subscribe(char* topic)
{
	
    if (connected) 
    {
        uint8_t var_header_topic[] = {0,10};    
        uint8_t fixed_header_topic[] = {MQTTSUBSCRIBE,sizeof(var_header_topic)+strlen(topic)+3};
        
        //utf topic
        uint8_t utf_topic[strlen(topic)+3];
        strcpy((char *)&utf_topic[2], topic);
		
        utf_topic[0] = 0;
        utf_topic[1] = strlen(topic);
        utf_topic[sizeof(utf_topic)-1] = 0;
        
        char packet_topic[sizeof(var_header_topic)+sizeof(fixed_header_topic)+strlen(topic)+3];
        memset(packet_topic,0,sizeof(packet_topic));
        memcpy(packet_topic,fixed_header_topic,sizeof(fixed_header_topic));
        memcpy(packet_topic+sizeof(fixed_header_topic),var_header_topic,sizeof(var_header_topic));
        memcpy(packet_topic+sizeof(fixed_header_topic)+sizeof(var_header_topic),utf_topic,sizeof(utf_topic));
        
        if (!send_data(packet_topic, sizeof(packet_topic)))
        {
            return -1;
        }
        return 1;
    }
    return -1;
}

void MQTTClient::live() 
{
    if (connected) 
    {
        int t = timer.read_ms();
        if (t - lastActivity > KEEPALIVE) {
            //Send 192 0 to broker
            printf("Send 192\r\n");
            char packet_192[] = {0xc0, 0x00};
            send_data((char*)packet_192, 2);
            lastActivity = t;
        }
    }
}

void MQTTClient::onTCPSocketEvent(TCPSocketEvent e)
{
    switch(e)
    {
        case TCPSOCKET_ACCEPT:
            printf("New TCPSocketEvent: TCPSOCKET_ACCEPT\r\n");
            break;
        case TCPSOCKET_CONNECTED: 
            printf("New TCPSocketEvent: TCPSOCKET_CONNECTED\r\n");
            connected = true;
            break;
        case TCPSOCKET_WRITEABLE: 
            printf("New TCPSocketEvent: TCPSOCKET_WRITEABLE\r\n");  
            break;
        case TCPSOCKET_READABLE:
            printf("New TCPSocketEvent: TCPSOCKET_READABLE\r\n");
            if(!sessionOpened)
            {
                read_open_session();
            }
            read_data();
            break;
        case TCPSOCKET_CONTIMEOUT:
            printf("New TCPSocketEvent: TCPSOCKET_CONTIMEOUT\r\n");
            break;
        case TCPSOCKET_CONRST:
            printf("New TCPSocketEvent: TCPSOCKET_CONRST\r\n");
            break;
        case TCPSOCKET_CONABRT:
            printf("New TCPSocketEvent: TCPSOCKET_CONABRT\r\n");
            break;
        case TCPSOCKET_ERROR:
            printf("New TCPSocketEvent: TCPSOCKET_ERROR\r\n");
            break;
        case TCPSOCKET_DISCONNECTED:
            printf("New TCPSocketEvent: TCPSOCKET_DISCONNECTED\r\n");
            pTCPSocket->close();
            connected = false;
            break;
    }
}