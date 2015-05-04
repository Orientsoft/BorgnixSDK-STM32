/*
 * File      : application.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-01-05     Bernard      the first version
 */

/**
 * @addtogroup STM32
 */
/*@{*/

#include <stdio.h>

#include "stm32f4xx.h"
#include <board.h>
#include <rtthread.h>

#ifdef RT_USING_LWIP
#include <lwip/sys.h>
#include <lwip/api.h>
#include <netif/ethernetif.h>
#include "stm32f4xx_eth.h"
#endif

#ifdef RT_USING_DFS
#include <dfs_fs.h>
#include <dfs_elm.h>
// #include "fs_test.h"
extern int dfs_init(void);
extern void mqtt_test(void);
// extern void mqtt_yeelink(void);
#endif

void rt_init_thread_entry(void* parameter)
{
	/*
	uint32_t* p1 = (uint32_t*)CCMDATARAM_BASE;
	uint32_t* p2 = (uint32_t*)(CCMDATARAM_BASE + sizeof(uint32_t));
	uint32_t* p3 = (uint32_t*)(CCMDATARAM_BASE + 2 * sizeof(uint32_t));
	*p1 = 1;
	*p2 = 2;
	*p3 = *p1 + *p2;
	rt_kprintf("p3 from CCM: %d\n", *p3);
	*/
	
  /* LwIP Initialization */
	#ifdef RT_USING_LWIP
	{
			extern void lwip_sys_init(void);

			/* register ethernetif device */
			eth_system_device_init();

			rt_hw_stm32_eth_init();
			/* re-init device driver */
			rt_device_init_all();

			/* init lwip system */
			lwip_sys_init();
			rt_kprintf("TCP/IP initialized!\n");
		
			mqtt_test();
			// mqtt_yeelink();
	}
	#endif

  //FS
	#ifdef RT_USING_DFS
	{
		dfs_init();
		
		#ifdef RT_USING_DFS_ELMFAT
		elm_init();
		if (dfs_mount("sd0", "/", "elm", 0, 0) == 0)
			rt_kprintf("File System initialized!\n");
		else
			rt_kprintf("File System init failed!\n");
		#endif
	}
	// file_test();
	#endif

  //GUI
}

int rt_application_init()
{
    rt_thread_t init_thread;

#if (RT_THREAD_PRIORITY_MAX == 32)
    init_thread = rt_thread_create("init",
                                   rt_init_thread_entry, RT_NULL,
                                   4096, 8, 20);
#else
    init_thread = rt_thread_create("init",
                                   rt_init_thread_entry, RT_NULL,
                                   4096, 80, 20);
#endif

    if (init_thread != RT_NULL)
        rt_thread_startup(init_thread);

    return 0;
}

/*@}*/
