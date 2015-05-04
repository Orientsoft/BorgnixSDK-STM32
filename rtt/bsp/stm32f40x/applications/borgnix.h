#ifndef BORGNIX_H
#define BORGNIX_H

// Beware! This is device limit! 256 devices will eat 1KB of SRAM
#define BORG_DEV_MAX_NUM 256

typedef struct _BorgDev BorgDev;
typedef void (*BorgDevCB)(BorgDev* me, cJSON* payload);

// static
extern char* borgHost;
extern int borgPort;
extern BorgDev* borgDevList[BORG_DEV_MAX_NUM];
// init borgnix SDK
void borg_dev_init(char* host, int port, char* uuid, char*token);

// BorgDev class
struct _BorgDev
{
	char* uuid;
	char* token;
	char* pubTopic;
	char* subTopic;
	BorgDevCB cb;
};

// public
// user should alloc memory for char* with rt_malloc(),
// and leave it to be freed by Borgnix API
BorgDev* borg_dev_create(char* uuid, char* token, BorgDevCB borgDevCB);
void borg_dev_destory(BorgDev* me);

int borg_dev_connect(BorgDev* me);
void borg_dev_disconnect(BorgDev* me);
void borg_dev_send(BorgDev* me, cJSON* payload);

#endif
