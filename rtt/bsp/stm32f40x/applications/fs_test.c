#include <rtthread.h>
#include <dfs_posix.h>
#include "stm32f4_discovery.h"
#include "fs_test.h"

void file_test()
{
	char data[120];
	
	int fd;
	int index, length;
	
	fd = open("/test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0);
	if (fd < 0)
	{
		rt_kprintf("open failed\n");
		return;
	}
	
	for (index = 0; index < sizeof(data); index++)
	{
		data[index] = index + 27;
	}
	
	length = write(fd, data, sizeof(data));
	if (length != sizeof(data))
	{
		rt_kprintf("write failed\n");
		close(fd);
		return;
	}
	
	rt_kprintf("file_test done\n");
	
	close(fd);
}
