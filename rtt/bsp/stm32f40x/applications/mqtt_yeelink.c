#include <rtthread.h>
#include <lwip/netdb.h> 
#include <lwip/sockets.h>
//#include <led.h>

#include "MQTTPacket.h"

#define     YEELINK
#ifdef YEELINK
#define     HOSTNAME    "z.borgnix.com"
#define     TOPIC       "hello/up"
#define     USERNAME    "dc290f10-ed5c-11e4-8e23-036c760b6ad0"
#define     PASSWORD    "3a890e70e7d64e313d7da4cd6bd826e034285e4a"
#else
#define     HOSTNAME    "192.168.1.108"
#define     TOPIC       "sensor"
#define     USERNAME    "xukai"
#define     PASSWORD    "871105"
#endif

int getdata(unsigned char* buf, int count);

#if 0
int Socket_new(char* addr, int port, int* sock)
{
    int type = SOCK_STREAM;
    struct sockaddr_in address;
#if defined(AF_INET6)
    struct sockaddr_in6 address6;
#endif
    int rc = -1;
#if defined(WIN32)
    short family;
#else
    u8_t family = AF_INET;      // modify by xukai
#endif
    struct addrinfo *result = NULL;
    struct addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};

    *sock = -1;
    if (addr[0] == '[')
      ++addr;

    if ((rc = getaddrinfo(addr, NULL, &hints, &result)) == 0) {
        struct addrinfo* res = result;

        /* prefer ip4 addresses */
        while (res) {
            if (res->ai_family == AF_INET) {
                result = res;
                break;
            }
            res = res->ai_next;
        }

#if defined(AF_INET6)
        if (result->ai_family == AF_INET6) {
            address6.sin6_port = htons(port);
            address6.sin6_family = family = AF_INET6;
            address6.sin6_addr = ((struct sockaddr_in6*)(result->ai_addr))->sin6_addr;
        }
        else
#endif
        if (result->ai_family == AF_INET) {
            address.sin_port = htons(port);
            address.sin_family = family = AF_INET;
            address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
        }
        else
            rc = -1;

        freeaddrinfo(result);
    }

    if (rc == 0) {
        *sock = socket(family, type, 0);
        if (*sock != -1) {
            if (family == AF_INET)
                rc = connect(*sock, (struct sockaddr*)&address, sizeof(address));
    #if defined(AF_INET6)
            else
                rc = connect(*sock, (struct sockaddr*)&address6, sizeof(address6));
    #endif
        }
    }
    return rc;
}
#endif

int yeelink_sockID = 0;     // 全局变量

static rt_uint8_t ping_stack[512];
static struct rt_thread ping_thread;
static void ping_thread_entry(void* parameter)
{
    static rt_uint8_t count;
    
    unsigned char buf[64];
    int buflen = sizeof(buf);
    int len;
    
    while (1)
    {
        rt_thread_delay( RT_TICK_PER_SECOND*10 ); 
        if (count % 2) {

        } else {

        }
        count++;
        
        len = MQTTSerialize_pingreq(buf, buflen);
        send(yeelink_sockID, buf, len, 0);
//        if (MQTTPacket_read(buf, buflen, getdata) == PINGRESP) {
//            rt_kprintf("PINGRESP\n");
//        }
    }
}

rt_err_t thread_ping_init()
{
    rt_err_t result;

    result = rt_thread_init(&ping_thread,
                            "ping",
                            ping_thread_entry,
                            RT_NULL,
                            (rt_uint8_t*)&ping_stack[0],
                            sizeof(ping_stack),
                            21,
                            5);
    if (result == RT_EOK) {
        rt_thread_startup(&ping_thread);
        rt_kprintf("ping thread start\n");
    }
    
    return result;
}

int yeelink_mqtt_connect(char* hostname, int port, int *psock)
{
    struct hostent *yeelink_host;
    struct in_addr yeelink_ipaddr;              // yeelink服务器IP地址，二进制形式
    struct sockaddr_in yeelink_sockaddr;        // yeelink服务器IP信息 包括端口号、IP版本和IP版本
    
    // 第一步 DNS地址解析
    yeelink_host = gethostbyname(hostname);  
    yeelink_ipaddr.s_addr = *(unsigned long *) yeelink_host->h_addr_list[0]; 
    rt_kprintf("Yeelink IP Address:%s\r\n" , inet_ntoa(yeelink_ipaddr));  
    
    // 第二步 创建套接字 
    if ((*psock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        rt_kprintf("Socket error\n"); 
        return 1;
    }
    
    yeelink_sockaddr.sin_family = AF_INET;
    yeelink_sockaddr.sin_port = htons(port);
    yeelink_sockaddr.sin_addr = yeelink_ipaddr;
    rt_memset(&(yeelink_sockaddr.sin_zero), 0, sizeof(yeelink_sockaddr.sin_zero));
    // 第三步 连接yeelink 
    if (connect(*psock, (struct sockaddr *)&yeelink_sockaddr, 
                sizeof(struct sockaddr)) == -1) {
        rt_kprintf("Connect Fail!\n");
        return 2;
    }

    return 0;   // 返回0代表成功
}


int getdata(unsigned char* buf, int count) {
    return recv(yeelink_sockID, buf, count, 0);
}

void mqtt_yeelink(void)
{
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    int rc = 0;
    unsigned char buf[200];
    int buflen = sizeof(buf);
    int msgid = 1;
    MQTTString topicString = MQTTString_initializer;
    int req_qos = 0;
    char* payload = "himypayload";
    int payloadlen = strlen(payload);
    int len = 0;
    int tv = 1000;
    int ret = 0;

    if ((rc = yeelink_mqtt_connect(HOSTNAME, 1883, &yeelink_sockID)) < 0)
        goto exit;
    
    data.clientID.cstring = "xukai871105";
    data.keepAliveInterval = 60;
    data.cleansession = 1;
    data.username.cstring = USERNAME;
    data.password.cstring = PASSWORD;

    // 设置接收超时时间
    setsockopt(yeelink_sockID, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(int));

    len = MQTTSerialize_connect(buf, buflen, &data);
		rt_kprintf("MQTT: conn - %s\r\n", buf);
    rc = send(yeelink_sockID, buf, len, 0);

    /* wait for connack */
    if (MQTTPacket_read(buf, buflen, getdata) == CONNACK) {
        unsigned char sessionPresent, connack_rc;

        if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0) {
            rt_kprintf("Unable to connect, return code %d\n", connack_rc);
            goto exit;
        } else {
            rt_kprintf("Connect OK\n", connack_rc);   
            // 建立心跳任务
            thread_ping_init();
        }
    }
    else
        goto exit;

    // 订阅主题
    topicString.cstring = TOPIC;
    len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);
    
    rc = send(yeelink_sockID, buf, len, 0);
    if (MQTTPacket_read(buf, buflen, getdata) == SUBACK) {
        unsigned short submsgid;
        int subcount;
        int granted_qos;

        rc = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, buf, buflen);
        if (granted_qos != 0) {
            rt_kprintf("granted qos != 0, %d\n", granted_qos);
            goto exit;
        } else {
            rt_kprintf("Subscribe OK\n");
        }
    }
    else
        goto exit;

    //while (RT_TRUE)
			{
        
        // if (MQTTPacket_read(buf, buflen, getdata) == PUBLISH) {
        ret = MQTTPacket_read(buf, buflen, getdata);
        if (ret == PUBLISH) {
            unsigned char dup;
            int qos;
            unsigned char retained;
            unsigned short msgid;
            int payloadlen_in;
            unsigned char* payload_in;
            int rc;
            MQTTString receivedTopic;

            rc = MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic,
                    &payload_in, &payloadlen_in, buf, buflen);
            //rt_kprintf("message arrived %.*s\n", payloadlen_in, payload_in);
        } else if (ret == PINGRESP) {
            rt_kprintf("PINGRESP\n");
        }
        
        // len = MQTTSerialize_publish(buf, buflen, 0, 0, 0, 0, topicString, (unsigned char*)payload, payloadlen);
        // rc = send(yeelink_sockID, buf, len, 0);
    }

    rt_kprintf("Disconnecting\r\n");
    len = MQTTSerialize_disconnect(buf, buflen);
    rc = send(yeelink_sockID, buf, len, 0);

exit:
    rc = shutdown(yeelink_sockID, SHUT_WR);
    rc = recv(yeelink_sockID, NULL, 0, 0);
    rc = closesocket(yeelink_sockID);

    return;
}

#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(mqtt_yeelink, Get Actutor Information From Yeelink Using MQTT Protocol);
#endif

