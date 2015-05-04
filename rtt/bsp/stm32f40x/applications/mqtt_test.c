#include <rtthread.h>
#include "cJSON.h"
#include "mqtt.h"
#include "borgnix.h"

void readCB(BorgDev* me, cJSON* payload)
{
	rt_kprintf("mqtt_test: sub %s\r\n", cJSON_Print(payload));
}

void mqtt_test()
{
	BorgDev* dev; 
	cJSON* payload = cJSON_CreateObject();
	
	// mqtt_connect("z.borgnix.com", 1883, "dc290f10-ed5c-11e4-8e23-036c760b6ad0", "3a890e70e7d64e313d7da4cd6bd826e034285e4a", (mqttReadCallback)&readCB);
	//mqtt_connect("voyager.orientsoft.cn", 11883, RT_NULL, RT_NULL, (mqttReadCallback)&readCB);
	//mqtt_pub("hello/up", "world");
	//mqtt_sub("hello/down");
	borg_dev_init("voyager.orientsoft.cn", 11883, "hello", RT_NULL);
	
	/*
	dev = borg_dev_create("hello1", RT_NULL, (BorgDevCB)&readCB);
	rt_kprintf("mqtt_test: 0\r\n");
	borg_dev_connect(dev);
	rt_kprintf("mqtt_test: 1\r\n");
	
	cJSON_AddStringToObject(payload, "hello1", "world");
	rt_thread_delay(RT_TICK_PER_SECOND * 3);
	rt_kprintf("mqtt_test: 2\r\n");
	borg_dev_send(dev, payload);
	rt_kprintf("mqtt_test: 3\r\n");
	*/
}

#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(mqtt_test, startup tcp client);
#endif
