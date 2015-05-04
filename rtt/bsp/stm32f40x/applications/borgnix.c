#include <time.h>
#include "cJSON.h"
#include "mqtt.h"
#include "borgnix.h"

// static
char* borgHost;
int borgPort;
char* borgUuid;
char* borgToken;

BorgDev* gate;
BorgDev* borgDevList[BORG_DEV_MAX_NUM];
int borgDevCount;

void borgRecvCB(char* topic, char* msg);
void gateCB(BorgDev* gate, cJSON* msg);
int connFlag = -1;

void borg_dev_init(char* host, int port, char* uuid, char*token)
{
	int ret;
	borgHost = host;
	borgPort = port;
	borgDevCount = 0;
	borgUuid = uuid;
	borgToken = token;
	
	if (connFlag == -1)
	{
		ret = mqtt_connect(borgHost, borgPort, borgUuid, borgToken, &borgRecvCB);
		if (ret)
		{
			connFlag = 1;
			gate = borg_dev_create(borgUuid, borgToken, &gateCB);
			borg_dev_connect(gate);
		}
	}
}

void gateCB(BorgDev* gate, cJSON* msg)
{
	// for now, do nothing
	rt_kprintf("gateCB: %s\r\n", cJSON_Print(msg));
}

// private
typedef void (*borgCB)(char* msg);

// public
BorgDev* borg_dev_create(char* uuid, char* token, BorgDevCB borgDevCB)
{
	int pubTopicLen = 0;
	int subTopicLen = 0;
	
	BorgDev* me = (BorgDev*)rt_malloc(sizeof(BorgDev));
	if (me != RT_NULL)
	{
		me->uuid = uuid;
		me->token = token;
		
		pubTopicLen = rt_strlen(uuid) + 3;
		me->pubTopic = (char*)rt_malloc(pubTopicLen);
		me->pubTopic[pubTopicLen - 1] = '\0';
		rt_memcpy(me->pubTopic, uuid, rt_strlen(uuid));
		rt_memcpy(me->pubTopic + rt_strlen(uuid), "/o", 2); 
		
		subTopicLen = rt_strlen(uuid) + 3;
		me->subTopic = (char*)rt_malloc(subTopicLen);
		me->subTopic[subTopicLen - 1] = '\0';
		rt_memcpy(me->subTopic, uuid, rt_strlen(uuid));
		rt_memcpy(me->subTopic + rt_strlen(uuid), "/i", 2); 
		
		me->cb = borgDevCB;
		
		borgDevList[borgDevCount] = me;
		borgDevCount++;
	}
	
	return me;
}

void borg_dev_destory(BorgDev* me)
{
	int i;
	int borgDevFlag = -1;
	
	if (me != RT_NULL)
	{
		rt_free(me->uuid);
		rt_free(me->token);
		rt_free(me->pubTopic);
		rt_free(me->subTopic);
		
		for (i = 0; i < borgDevCount; i++)
		{
			if (borgDevList[i] == me)
				borgDevFlag = 1;
			
			if (borgDevFlag && (i != borgDevCount - 1))
				borgDevList[i] = borgDevList[i + 1];
		}
		borgDevCount--;
	}
	
	rt_free(me);
}

int borg_dev_connect(BorgDev* me)
{
	if (connFlag)
	{
		mqtt_sub(me->subTopic);
		return 1;
	}
	else
	{
		return -1;
	}
}

void borg_dev_disconnect(BorgDev* me)
{
	// unsub
	mqtt_unsub(me->subTopic);
}

void borg_dev_send(BorgDev* me, cJSON* payload)
{
	char* buffer;
	// time_t ts;
	// add protocol data
	cJSON* data;
	
	data = cJSON_CreateObject();
	// time(&ts);
	// cJSON_AddNumberToObject(data, "time", 100);
	cJSON_AddStringToObject(data, "action", "data");
	cJSON_AddStringToObject(data, "ver", "1.0");
	cJSON_AddItemToObject(data, "payload", payload);
	buffer = cJSON_Print(data);
	
	rt_kprintf("borg_dev_send: [%s] %s\r\n", me->pubTopic, buffer);
	mqtt_pub(me->pubTopic, buffer);
}

// private
void borgRecvCB(char* topic, char* msg)
{
	int i;
	cJSON* data;
	cJSON* payloadJson;
	
	// 1. parse json
	data = cJSON_Parse(msg);
	
	// 2. deal with topic
	for (i = 0; i < rt_strlen(topic); i++)
	{
		if (topic[i] == '/')
			topic[i] = '\0';
	}
	
	payloadJson = cJSON_GetObjectItem(data, "payload");
	
	// 2. dispatch (call device callback)
	for (i = 0; i < borgDevCount; i++)
	{
		if (0 == rt_strcmp(topic, borgDevList[i]->uuid))
		{
			borgDevList[i]->cb(borgDevList[i], payloadJson);
		}
	}
}
