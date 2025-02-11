/** *************************************************************************
 *  @file          main_l2switch.c
 *  @brief	  基于FAST架构的软件二层交换示例程序
 *
 *  详细说明
 *
 *  @date	   2017/04/08 10:39:17 星期六
 *  @author		XDL(Copyright  2017  XuDongLai)
 *  @email		<XuDongLai0923@163.com>
 *  @version	0.2.0
 ****************************************************************************/
/*
 * main_l2switch.c
 *
 * Copyright (C) 2017 - XuDongLai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "fast.h"
#include <unistd.h>

#define NM08_PORT_CNT 4	   /*设备端口数量*/
#define NM08_NEIGH_MAX 128 /*每个端口上最大存储邻居MAC个数*/
#define MAC_LEN 6		   /*MAC地址长度*/
#define MAC_LIFE_TIME 10   /*MAC地址生命时长为300秒，可调整*/

#define COUNT_MAX 500 // 端口转发计数阈值

int debug = 0;
#define xprintf(argc...) \
	if (debug)           \
		printf(argc);
/*端口计数*/
struct nm08_port_stats
{
	u64 recv_pkts;
	u64 recv_bytes;
	u64 send_pkts;
	u64 send_bytes;
	u64 count; // 端口转发计数
}; /*32B*/

/*MAC地址，有效位标记和MAC地址学习或更新时间*/

struct nm08_port_mac
{
	u8 port;		   /*地址所在端口号信息*/
	u8 valid;		   /*地址有效位标记*/
	u8 addr[MAC_LEN];  /*存储MAC地址*/
	struct timeval tv; /*存储此MAC的开始有效时间*/
}; /*24B*/

/*08的邻居信息表*/
struct nm08_neigh_table
{
	struct nm08_port_stats port[NM08_PORT_CNT];
	struct nm08_port_mac mac[NM08_NEIGH_MAX];
};

struct nm08_neigh_table *nm08_table = NULL;

/**
 * @brief
 *
 * @param addr1
 * @param addr2
 *
 * @return
 */
int ether_addr_equal(u8 *addr1, u8 *addr2)
{
	u16 *a = (u16 *)addr1;
	u16 *b = (u16 *)addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}

/**
 * @brief
 *
 * @param inport
 * @param index
 */
/*更新MAC地址对应的有效时间*/
void update_mac_time(u8 inport, u8 index)
{
	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);
	tv_now.tv_sec += MAC_LIFE_TIME;		/*MAC地址生命周期设置，每次更新，此MAC地址有效时长为300秒，用户可调整*/
	nm08_table->mac[index].tv = tv_now; /*更新当前时间*/
	if (nm08_table->mac[index].port != inport)
	{
		/*机器换了端口*/
	}
	nm08_table->mac[index].port = inport; /*更新MAC所在端口号，有可能是从另外端口拔出，插到了新端口上，如此可以快速更新*/
	xprintf("update_mac_time->port:%d,index:%d\n", inport, index);
}

/**
 * @brief
 *
 * @param inport
 * @param src_mac
 *
 * @details
 * Address learning process, which learns the source MAC address of the packet
 * and stores it in the corresponding port MAC table.
 */
void learn_smac(u8 inport, u8 *src_mac)
{
	/* Search for the first empty slot in the MAC table */
	int i = 0, j = -1;

	xprintf("learn_smac->\n");

	// TODO User add code

	for (; i < NM08_NEIGH_MAX; i++)
	{
		if (nm08_table->mac[i].valid == 1)
		{
			/* If the source MAC address is already in the table, update the time */
			if (!ether_addr_equal(&nm08_table->mac[i].addr[0], src_mac))
			{
				xprintf("learn_smac->Update TIME!\n");
				update_mac_time(inport, i);
				return;
			}
		}
		else if (j == -1)
			j = i; /* Record the first empty slot */
	}

	/* If no empty slot is found, return without doing anything */
	if (j == -1)
	{
		xprintf("learn_smac->No space!\n");
		return;
	}

	/* Store the source MAC address in the first empty slot */
	memcpy(&nm08_table->mac[j].addr[0], src_mac, MAC_LEN);
	nm08_table->mac[j].valid = 1;
	update_mac_time(inport, j);


	xprintf("learn_smac->add new MAC,port:%d,index:%d\n", inport, j);
}

/**
 * @brief Find the output port of the destination MAC address
 *
 * @param inport The port number of the incoming packet
 * @param dst_mac The destination MAC address
 *
 * @return The output port number of the destination MAC address,
 *         -1 means the MAC address is not found in the table
 */
int find_dmac(u8 inport, u8 *dst_mac)
{
	int ret = -1, i = 0;

	// TODO User add code

	// Search the MAC address table to find the output port
	for (; i < NM08_NEIGH_MAX; i++)
	{
		// Check the validity and port number of the current MAC entry
		if (nm08_table->mac[i].valid == 1 && nm08_table->mac[i].port != inport)
		{
			// Compare the destination MAC address with the current MAC entry
			if (!ether_addr_equal(&nm08_table->mac[i].addr[0], dst_mac))
			{
				// If the MAC address is found, return the output port number
				ret = nm08_table->mac[i].port;
				break;
			}
		}
	}


	// Print the result of the search
	xprintf("find_dmac->ret = %d\n", ret);
	return ret; /* Return -1 if the MAC address is not found in the table */
}

/**
 * @brief
 *
 * @param pkt
 * @param pkt_len
 */
void pkt_send_normal(struct fast_packet *pkt, int pkt_len)
{
	xprintf("pkt_send_normal->%p,outport:%d,len:%d\n", pkt, pkt->um.outport, pkt_len);
	pkt->um.pktsrc = 1; /*报文来源为CPU输入*/
	pkt->um.pktdst = 0; /*报文目的为硬件输出*/
	pkt->um.dstmid = 5; /*直接从硬件GOE模块输出，不走解析、查表等模块*/
	fast_ua_send(pkt, pkt_len);
}

/**
 * @brief
 *
 * @param pkt
 * @param pkt_len
 */
void pkt_send_flood(struct fast_packet *pkt, int pkt_len)
{
	int i = 0, inport = pkt->um.inport; /*保存输入端口*/
	xprintf("------pkt_send_flood------\n");

	// TODO User add code

	// 遍历所有端口（假设NM08_PORT_CNT是定义的端口总数）
	for (; i < NM08_PORT_CNT; i++)
	{
		// 如果当前端口不是输入端口，则发送数据包到该端口
		if (i != inport && nm08_table->port->count < COUNT_MAX)
		{
			// 设置数据包的输出端口为当前端口
			pkt->um.outport = i;

			// 调用pkt_send_normal函数发送数据包
			pkt_send_normal(pkt, pkt_len);
		}
	}

}

/**
 * @brief
 */
void nm08_show_mac_info(void)
{
	int i = 0, j = 0, max_cnt = 0;
	char buf[400];
	int port_mac_cnt[NM08_PORT_CNT] = {0};
	struct nm08_port_mac mac[NM08_PORT_CNT][NM08_NEIGH_MAX] = {{0}};

	for (; i < NM08_NEIGH_MAX; i++)
	{
		if (nm08_table->mac[i].valid == 1)
		{
			mac[nm08_table->mac[i].port][port_mac_cnt[nm08_table->mac[i].port]] = nm08_table->mac[i];
			port_mac_cnt[nm08_table->mac[i].port]++;
		}
	}
	/*记录一个端口最多的MAC地址数量，确定我们最大需要输出几行*/
	max_cnt = port_mac_cnt[0];
	for (i = 1; i < NM08_PORT_CNT; i++)
	{
		if (port_mac_cnt[i] > max_cnt)
			max_cnt = port_mac_cnt[i];
	}
	max_cnt++; /*最后多打印一行空行*/
	xprintf("\nID               PORT0              PORT1              PORT2              PORT3\n");

	for (j = 0; j < max_cnt; j++)
	{
		sprintf(buf, "%2d ", j);
		for (i = 0; i < NM08_PORT_CNT; i++)
		{

			if (mac[i][j].valid == 1)
			{
				sprintf(buf, "%s  %02X:%02X:%02X:%02X:%02X:%02X", buf,
						mac[i][j].addr[0],
						mac[i][j].addr[1],
						mac[i][j].addr[2],
						mac[i][j].addr[3],
						mac[i][j].addr[4],
						mac[i][j].addr[5]);
			}
			else
			{
				sprintf(buf, "%s                  .", buf);
			}
		}
		xprintf("%s\n", buf);
	}
}

/**
 * @brief
 *
 * @param argv
 *
 * @return
 */
/*MAC地址老化处理线程*/
void *nm08_mac_aging(void *argv)
{
	struct timeval tv_now;
	int i = 0, k = 0, aging_cnt = 0;

	while (1)
	{
		gettimeofday(&tv_now, NULL);

		// TODO User add code
		for (i = 0; i < NM08_NEIGH_MAX; i++)
		{
			// 如果有效时间已到，即当前时间大于MAC地址的有效时间，并且该MAC地址是有效的
			if (tv_now.tv_sec > nm08_table->mac[i].tv.tv_sec && nm08_table->mac[i].valid == 1)
			{
				nm08_table->mac[i].valid = 0; // 将该MAC地址标记为无效
				aging_cnt++;				  // 增加老化计数
			}
		}
		xprintf("aging[%d]->invalid.mac:%d\n", k, aging_cnt++); // 打印老化信息，包括当前的k值和老化的MAC地址数量
		k++;													// 增加k值
		aging_cnt = 0;											// 重置老化计数

		usleep(5 * 1000 * 1000); /*老化时间误差，每10秒才判断一次。如果提高精度可缩短时间*/
		nm08_show_mac_info();
	}
}

/**
 * @brief
 *
 * @param
 *
 * @return
 */
/*端口计数老化处理线程*/
void *nm08_count_aging()
{
	while (1)
	{
		usleep(500 * 1000 * 1000);

		for (int i = 0; i < NM08_PORT_CNT; i++)
		{
			nm08_table->port->count = 0;
		}
	}
}

/**
 * @brief
 */
void nm08_start_aging(void)
{
	pthread_t tid;

	/*创建地址老化处理线程*/
	if (pthread_create(&tid, NULL, nm08_mac_aging, NULL))
	{
		xprintf("Create nm08_mac_aging thread error!\n");
		exit(0);
	}
	else
	{
		xprintf("Create nm08_mac_aging thread OK!\n");
	}
}

void nm08_start_count_aging()
{
	pthread_t tid;

	/*创建端口计数老化处理线程*/
	if (pthread_create(&tid, NULL, nm08_count_aging, NULL))
	{
		xprintf("Create nm08_count_aging thread error!\n");
		exit(0);
	}
	else
	{
		xprintf("Create nm08_count_aging thread OK!\n");
	}
}

/**
 * @brief
 *
 * @param pkt
 * @param pkt_len
 *
 * @return
 */
int callback(struct fast_packet *pkt, int pkt_len)
{
	int outport = -1;

	xprintf("inport:%d,dstmid:%d,len:%d,dmac:%02X:%02X:%02X:%02X:%02X:%02X,smac:%02X:%02X:%02X:%02X:%02X:%02X\n", pkt->um.inport, pkt->um.dstmid, pkt_len,
			pkt->data[0], pkt->data[1], pkt->data[2], pkt->data[3], pkt->data[4], pkt->data[5],
			pkt->data[6], pkt->data[7], pkt->data[8], pkt->data[9], pkt->data[10], pkt->data[11]);

	// print_pkt(pkt,pkt_len);
	// pkt->um.outport = 1;
	// pkt_send_normal(pkt,pkt_len);/*正常发送报文*/
	// return 0;
	/*MAC地址学习过程*/
	learn_smac(pkt->um.inport, &pkt->data[MAC_LEN]); /*用源MAC位置开始学习*/

	/*查表过程*/
	outport = find_dmac(pkt->um.inport, pkt->data); /*用目的MAC地址开始查表*/

	/*发送报文*/
	if (outport == -1) /*报文需要泛洪操作*/
	{
		pkt_send_flood(pkt, pkt_len); /*泛洪发送，保留输入端口不变调用*/
	}
	else /*正常转发*/
	{
		pkt->um.outport = outport;	   /*修改报文输出端口号*/
		pkt_send_normal(pkt, pkt_len); /*正常发送报文*/
	}
	return 0;
}

/**
 * @brief
 */
void ua_init(u8 mid)
{
	int ret = 0;
	/*向系统注册，自己进程处理报文模块ID为1的所有报文*/
	if ((ret = fast_ua_init(mid, callback))) // UA模块实例化(输入参数1:接收模块ID号,输入参数2:接收报文的回调处理函数)
	{
		perror("fast_ua_init!\n");
		exit(ret); // 如果初始化失败,则需要打印失败信息,并将程序结束退出!
	}
}

/**
 * @brief
 *
 * @param argc
 * @param argv[]
 *
 * @return
 */
int main(int argc, char *argv[])
{
	u8 mid = FAST_UA_DFT_L2SWITCH;
	debug = 1;
	if (argc == 2)
	{
		debug = atoi(argv[1]);
	}
	else if (argc == 3)
	{
		debug = atoi(argv[1]);
		mid = atoi(argv[2]);
	}

	if (mid < 129)
	{
		printf("Usage:\n\t%s debug mid\n", argv[0]);
		printf("\tdeubg[0,1],UA mid rang [129,255]!\n");
		exit(0);
	}
	/*申请地址表存储空间*/
	nm08_table = (struct nm08_neigh_table *)malloc(sizeof(struct nm08_neigh_table));
	/*空间清零*/
	memset(nm08_table, 0, sizeof(struct nm08_neigh_table));

	/*UA模块初始化	*/
	ua_init(mid);

	/*配置硬件规则，将硬件所有报文送到模块ID为mid的进程处理*/
	fast_reg_wr(FAST_ACTION_REG_ADDR | FAST_DEFAULT_RULE_ADDR, ACTION_SET_MID << 28 | mid);

	/*启动地址学习表老化线程*/
	nm08_start_aging();

	// 启动端口转发计数老化线程
	nm08_start_count_aging();

	/*启动线程接收分派给UA进程的报文*/
	fast_ua_recv();

	/*主进程进入循环休眠中,数据处理主要在回调函数*/
	while (1)
	{
		pause();
	}
	return 0;
}
