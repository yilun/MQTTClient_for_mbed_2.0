#include "mbed.h"
#include "EthernetNetIf.h"
#include "MQTTClient.h"

EthernetNetIf ethernet;    

IpAddr serverIpAddr(10,1,1,5);  /*Sever ip address*/

void callback(char* topic, char* payload); /*Callback function prototype*/

MQTTClient mqtt(serverIpAddr, 1883, callback);

void callback(char* topic, char* payload)
{
    printf("Topic: %s\r\n", topic);
    printf("Payload: %s\r\n\r\n", payload);
    //Send incoming payloads back to topic "/mbed".
    mqtt.publish("/mbed", payload);
}

int main() {
	
    printf("\r\n############### MQTTClient Tester  ##＃＃#######\r\n\r\n");
    
    EthernetErr ethErr = ethernet.setup();
    if(ethErr){
        printf("Ethernet Error %d\r\n", ethErr);  
    } else {
        printf("mbed is online...\r\n");
    }
	
    char clientID[] = "mbed";   /*Client nanme show for MQTT server*/
    char pub_topic[] = "/mbed";   /*Publish to topic : "/mbed" */
    char sub_topic[] = "/mirror";   /*Subscribe to topic : "/mirror" */
	
    if(mqtt.connect(clientID)){
        printf("\r\nConnect to server sucessed ..\r\n");
    } else {
        printf("\r\nConnect to server failed ..\r\n");
        return -1;
    }
    
    mqtt.publish(pub_topic, "Hello here is mbed...");
    //printf("\r\nPlease swape your card..\r\n");
    
    mqtt.subscribe(sub_topic);
    
    int flag = 0;
    /*Keep alive for 300s or 5mins*/
    while(flag < 300){
        Net::poll();
        wait(1);
        flag++;
        mqtt.live();
    }
    
    mqtt.disconnect();
    
    printf("#### End of the test.. ####\r\n\r\n");
}