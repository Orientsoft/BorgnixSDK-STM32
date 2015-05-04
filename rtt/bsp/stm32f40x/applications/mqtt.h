#ifndef MQTT_H
#define MQTT_H

typedef void (*mqttReadCallback)(char* topic, char* string);

// public member
extern char* mqttHost;
extern int mqttPort;
extern char* mqttUser;
extern char* mqttPass;

extern volatile int mqttSocket;
extern int mqttSocketTimeout;

extern mqttReadCallback mqttReadCB;

// interface
int mqtt_connect(char* host, int port, char* username, char* password, mqttReadCallback readCB);
void mqtt_disconnect(void);

int mqtt_pub(char* topic, char* string);
int mqtt_sub(char* topic);
int mqtt_unsub(char* topic);

#endif
