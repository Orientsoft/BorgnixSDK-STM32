#include <rtthread.h>
#include <lwip/netdb.h> 
#include <lwip/sockets.h>

#include "MQTTPacket.h"
#include "mqtt.h"

// public member
char* mqttHost;
int mqttPort;
char* mqttUser;
char* mqttPass;

volatile int mqttSocket;
int mqttSocketTimeout = 0;

mqttReadCallback mqttReadCB;

// private member
ALIGN(4)
static rt_uint8_t mqttPingThreadStack[1024];
static struct rt_thread mqttPingThread;
ALIGN(4)
static rt_uint8_t mqttReadThreadStack[2048];
static struct rt_thread mqttReadThread;

int pingInterval = 60; // in seconds
volatile static unsigned int lastPingTs = 0;
const unsigned int pingThreshold = 120; // in seconds
int msgid = 0;
int qos;

// helper function
int mqtt_reconnect(void);
void mqtt_socket_disconnect(void);
void mqttPingThreadEntry(void* param);
rt_err_t thread_ping_init(void);
void mqttReadThreadEntry(void* param);
rt_err_t thread_read_init(mqttReadCallback cb);
int getData(unsigned char* buf, int len);

int mqtt_connect(char* host, int port, char* username, char* password, mqttReadCallback readCB)
{
	int bufLen = 200;
	unsigned char* buf = (unsigned char*)rt_malloc(bufLen);
	
	struct hostent* brokerHost;
	struct in_addr brokerAddr;
	struct sockaddr_in brokerSockAddr;
	
	int len = 0;
	int code = 0;
	
	MQTTPacket_connectData mqttConnData = MQTTPacket_connectData_initializer;
	
	rt_kprintf("MQTT: mqtt_connect\r\n");
	
	if (mqttHost != NULL)
		free(mqttHost);
	mqttHost = (char*)rt_malloc(rt_strlen(host));
	strcpy(mqttHost, host);
	
	mqttPort = port;
	if (mqttUser != NULL)
		free(mqttUser);
	mqttUser = (char*)rt_malloc(rt_strlen(username));
	strcpy(mqttUser, username);
	
	if (mqttPass != NULL)
		free(mqttPass);
	mqttPass = (char*)rt_malloc(rt_strlen(password));
	strcpy(mqttPass, password);
	
	mqttConnData.username.cstring = username;
	mqttConnData.password.cstring = password;
	mqttConnData.clientID.cstring = "abc";
	mqttConnData.keepAliveInterval = 60;
	mqttConnData.cleansession = 1;
	
	mqttReadCB = readCB;

	// DNS
	brokerHost = gethostbyname(mqttHost);
	brokerAddr.s_addr = *(unsigned long*)brokerHost->h_addr_list[0];
	rt_kprintf("MQTT: broker ip - %s\r\n", inet_ntoa(brokerAddr));
	
	// socket
	if ((mqttSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		rt_kprintf("MQTT: socket error\r\n");
		return -1;
	}
	brokerSockAddr.sin_family = AF_INET;
	brokerSockAddr.sin_addr = brokerAddr;
	brokerSockAddr.sin_port = htons(mqttPort);
	rt_memset(&(brokerSockAddr.sin_zero), 0, sizeof(brokerSockAddr.sin_zero));
	
	if (connect(mqttSocket, (struct sockaddr *)&brokerSockAddr, sizeof(struct sockaddr)) == -1)
	{
		rt_kprintf("MQTT: socket connect error\r\n");
		return -2;
	}
	
	setsockopt(mqttSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&mqttSocketTimeout, sizeof(int));
	
	rt_kprintf("MQTT: sokcet connected\r\n");
	
	// mqtt conn
	len = MQTTSerialize_connect(buf, bufLen, &mqttConnData);
	code = send(mqttSocket, buf, len, 0);
	// connack
	if (MQTTPacket_read(buf, bufLen, getData) == CONNACK)
	{
		unsigned char sessionPresent;
		unsigned char connackCode;
		if (MQTTDeserialize_connack(&sessionPresent, &connackCode, buf, bufLen) != 1 || connackCode != 0)
		{
			rt_kprintf("MQTT: broker error, return code %d\r\n", connackCode);
			mqtt_socket_disconnect();
		}
		else
		{
			rt_kprintf("MQTT: broker connected\r\n");
			
			thread_read_init(mqttReadCB);
			thread_ping_init();
		}
	}
	else
		mqtt_socket_disconnect();
	
	rt_free(buf);
	
	return 1;
}

int mqtt_reconnect()
{
	int bufLen = 200;
	unsigned char* buf = (unsigned char*)rt_malloc(bufLen);
	
	struct hostent* brokerHost;
	struct in_addr brokerAddr;
	struct sockaddr_in brokerSockAddr;
	
	int len = 0;
	int code = 0;
	
	MQTTPacket_connectData mqttConnData = MQTTPacket_connectData_initializer;
	
	// DNS
	brokerHost = gethostbyname(mqttHost);
	brokerAddr.s_addr = *(unsigned long*)brokerHost->h_addr_list[0];
	rt_kprintf("MQTT: broker ip - %s\r\n", inet_ntoa(brokerAddr));
	
	// socket
	if ((mqttSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		rt_kprintf("MQTT: socket error\r\n");
		return -1;
	}
	brokerSockAddr.sin_family = AF_INET;
	brokerSockAddr.sin_addr = brokerAddr;
	brokerSockAddr.sin_port = htons(mqttPort);
	rt_memset(&(brokerSockAddr.sin_zero), 0, sizeof(brokerSockAddr.sin_zero));
	
	if (connect(mqttSocket, (const struct sockaddr *)&brokerSockAddr, sizeof(struct sockaddr)) == -1)
	{
		rt_kprintf("MQTT: socket connect error\r\n");
		return -2;
	}
	
	setsockopt(mqttSocket, SOL_SOCKET, SO_RCVTIMEO, &mqttSocketTimeout, sizeof(int));
	
	rt_kprintf("MQTT: sokcet connected\r\n");
	
	// mqtt conn
	len = MQTTSerialize_connect(buf, bufLen, &mqttConnData);
	code = send(mqttSocket, buf, len, 0);
	// connack
	if (MQTTPacket_read(buf, bufLen, getData) == CONNACK)
	{
		unsigned char sessionPresent;
		unsigned char connackCode;
		if (MQTTDeserialize_connack(&sessionPresent, &connackCode, buf, bufLen) != 1 || connackCode != 0)
		{
			rt_kprintf("MQTT: broker error, return code %d\r\n", connackCode);
			mqtt_socket_disconnect();
		}
		else
		{
			rt_kprintf("MQTT: broker connected\r\n");
			
			// we don't have to restart ping thread since it's not stopped
			rt_thread_startup(&mqttReadThread);
		}
	}
	else
		mqtt_socket_disconnect();
	
	rt_free(buf);
	
	return 1;
}

void mqtt_disconnect(void)
{
	int bufLen = 200;
	unsigned char* buf = (unsigned char*)rt_malloc(bufLen);
	
	int len = 0;
	int code = 0;
	
	// mqtt disconnect
	len = MQTTSerialize_disconnect(buf, bufLen);
	code = send(mqttSocket, buf, bufLen, 0);
	
	// socket disconnect
	mqtt_socket_disconnect();
	
	// TO DO : clear all resources
	
	rt_free(buf);
}

void mqtt_socket_disconnect(void)
{
	shutdown(mqttSocket, SHUT_WR);
	recv(mqttSocket, NULL, 0, 0);
	closesocket(mqttSocket);
	
	return;
}

int mqtt_pub(char* topic, char* string)
{
	int bufLen = 200;
	unsigned char* buf = (unsigned char*)rt_malloc(bufLen);
	
	int len = 0;
	int code = 0;
	MQTTString topicStr;
	
	topicStr.cstring = topic;	
	len = MQTTSerialize_publish(buf, bufLen, 0, 0, 0, 0, topicStr, (unsigned char*)string, rt_strlen(string));
	
	code = send(mqttSocket, buf, len, 0);
	
	// TO DO : convert code
	
	rt_free(buf);
	
	return 1;
}

int mqtt_sub(char* topic)
{
	int bufLen = 200;
	unsigned char* buf = (unsigned char*)rt_malloc(bufLen);
	
	int len = 0;
	int code = 0;
	MQTTString topicStr;
	
	char* topicBuf = (char*)rt_malloc(rt_strlen(topic) + 1);
	topicBuf[rt_strlen(topic)] = '\0';
	memcpy(topicBuf, topic, rt_strlen(topic));
	
	// sub 1 more topic
	topicStr.cstring = topicBuf;
	rt_kprintf("MQTT: Sub topic - %s\r\n", topic);
	rt_thread_delay(RT_TICK_PER_SECOND * 3);
	len = MQTTSerialize_subscribe(buf, bufLen, 0, msgid, 1, &topicStr, &qos);
	rt_thread_delay(RT_TICK_PER_SECOND * 3);
	rt_kprintf("MQTT: Sub topic - %s\r\n", topic);
	msgid++;
	rt_thread_delay(RT_TICK_PER_SECOND * 3);
	code = send(mqttSocket, buf, len, 0);
	rt_kprintf("MQTT: Sub topic - %s\r\n", topic);
	
	rt_free(buf);
	rt_free(topicBuf);

	return 1;
}

int mqtt_unsub(char* topic)
{
	int bufLen = 200;
	unsigned char* buf = (unsigned char*)rt_malloc(bufLen);
	
	int len = 0;
	int code = 0;
	MQTTString topicStr;
	
	char* topicBuf = (char*)rt_malloc(rt_strlen(topic) + 1);
	topicBuf[rt_strlen(topic)] = '\0';
	memcpy(topicBuf, topic, rt_strlen(topic));
	
	// sub 1 more topic
	topicStr.cstring = topicBuf;
	rt_kprintf("MQTT: Sub topic - %s\r\n", topic);
	
	len = MQTTSerialize_unsubscribe(buf, bufLen, 0, msgid, 1, &topicStr);
	rt_kprintf("MQTT: Sub topic - %s\r\n", topic);
	msgid++;
	code = send(mqttSocket, buf, len, 0);
	rt_kprintf("MQTT: Sub topic - %s\r\n", topic);
	
	rt_free(buf);
	rt_free(topicBuf);

	return 1;
}

void mqttPingThreadEntry(void* param)
{
	int len;
	unsigned int currentTs;
	
	unsigned char buf[200];
	int bufLen = sizeof(buf);
	
	len = MQTTSerialize_pingreq(buf, bufLen);
	while(1)
	{
		// get currentTs
		currentTs = rt_tick_get();
		send(mqttSocket, buf, len, 0);
		
		rt_kprintf("MQTT: ping interval - %d\r\n", currentTs - lastPingTs);
		if (currentTs - lastPingTs > pingThreshold * RT_TICK_PER_SECOND)
		{
			// reconnect
			// 1. stop mqttReadThread
			rt_thread_detach(&mqttReadThread);
			// 2. call mqtt_disconnect()
			mqtt_disconnect();
			// 3. call mqtt_connect()
			mqtt_reconnect();
			// TO DO : 4. re-sub all topics
		}
		
		rt_thread_delay(RT_TICK_PER_SECOND * pingInterval);
	}
}

rt_err_t thread_ping_init(void)
{
	rt_err_t result;
	
	result = rt_thread_init(&mqttPingThread, "MQTTPingThread", mqttPingThreadEntry, RT_NULL, (rt_uint8_t*)&mqttPingThreadStack[0], sizeof(mqttPingThreadStack), 21, 5);

	if (result == RT_EOK)
	{
		rt_thread_startup(&mqttPingThread);
		rt_kprintf("MQTT: ping thread started\r\n");
	}
	
	return result;
}

void mqttReadThreadEntry(void* param)
{
	int len;
	mqttReadCallback cb = (mqttReadCallback)param;
	int ret = 0;
	unsigned char buf[200];
	int bufLen = sizeof(buf);
	
	// suback
	unsigned short submsgid;
	int subcount;
	int grantedQos;
	
	while(RT_TRUE)
	{
		// read package
		ret = MQTTPacket_read(buf, bufLen, getData);
		switch (ret)
		{
			// handle publish data
			case PUBLISH:
			{
				unsigned char dup;
				int qos;
				unsigned char retained;
				unsigned short msgid;
				int payloadlen_in;
				unsigned char* payload_in;
				MQTTString receivedTopic;
				// callback
				MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic, &payload_in, &payloadlen_in, buf, bufLen);
				payload_in[payloadlen_in] = '\0';
				
				cb(receivedTopic.lenstring.data, (char*)payload_in);
				
				lastPingTs = rt_tick_get();
			}
			break;
			
			// handle ping resp
			case PINGRESP:
			// get current ts
			lastPingTs = rt_tick_get();
			rt_kprintf("MQTT: PINGRESP - %d\r\n", lastPingTs);
			break;
			
			// handle suback
			case SUBACK:
			ret = MQTTDeserialize_suback(&submsgid, 1, &subcount, &grantedQos, buf, bufLen);
			if (grantedQos != 0)
			{
				rt_kprintf("MQTT: granted qos != 0, %d\r\n", grantedQos);
				// TO DO : should we just call mqtt_socket_disconnect()?
				mqtt_disconnect();
			}
			else
			{
				rt_kprintf("MQTT: subscribe OK\r\n");
			}
			break;
			
			case -1:
				// nothing
			break;
			
			// handle other mqtt packets
			default:
				rt_kprintf("MQTT: unknown packet type, %d\r\n", ret);
			break;
		}
	}
}

rt_err_t thread_read_init(mqttReadCallback cb)
{
	rt_err_t result;
	
	result = rt_thread_init(&mqttReadThread, "MQTTReadThread", mqttReadThreadEntry, (void*)cb, (rt_uint8_t*)&mqttReadThreadStack[0], sizeof(mqttReadThreadStack), 21, 5);
	if (result == RT_EOK)
	{
		rt_thread_startup(&mqttReadThread);
		rt_kprintf("MQTT: read thread started\r\n");
	}
	
	return result;
}

int getData(unsigned char* buf, int count)
{
	int ret = recv(mqttSocket, buf, count, 0);
	
	return ret;
}
