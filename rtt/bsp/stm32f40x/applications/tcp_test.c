#include <rtthread.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

ALIGN(4)
static const char send_data[] = "This is TCP Client from RT-Thread.\n";
void tcpclient(const char* url, int port)
{
char *recv_data;
struct hostent *host;
int sock, bytes_received;
struct sockaddr_in server_addr;
	
rt_kprintf("tcpclient\n");

host = gethostbyname(url);
rt_kprintf("Host: %d.%d.%d.%d\n", host->h_addr[0], host->h_addr[1], host->h_addr[2], host->h_addr[3]);
	
recv_data = rt_malloc(1024);
if (recv_data == RT_NULL)
{
rt_kprintf("No memory\n");
return;
}

if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
{
rt_kprintf("Socket error\n");
rt_free(recv_data);
return;
}

server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(port);
server_addr.sin_addr = *((struct in_addr *) host->h_addr);
rt_memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

if (connect(sock, (struct sockaddr *) &server_addr,
sizeof(struct sockaddr)) == -1)
{
rt_kprintf("Connect error\n");
rt_free(recv_data);
return;
}

send(sock, send_data, strlen(send_data), 0);
send(sock, send_data, strlen(send_data), 0);

closesocket(sock);

return;
}

#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(tcpclient, startup tcp client);
#endif
