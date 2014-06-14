/*
 * Copyright (C) 2014 Engicam srl
 *
 * Configuration parameters for the Engicam i.CoreM6 RQS and QSEVEN
 * module. Refer for all default parameter to this file
 */

#ifndef __COMMON_PARAMETER_H
#define __COMMON_PARAMETER_H


	/* NETWORK SETTINGS */

	#define CONFIG_SERVERIP		192.168.2.96
	#define CONFIG_IPADDR		192.168.2.75
	#define CONFIG_NETMASK		255.255.255.0


	/* Common parameter
	 * For all modules SODIMM and QSEVEN
	 */
	#define COMMON_PARAMETER 			\
		"netdev=eth0\0" 			\
		"ethprime=FEC0\0" 			\
		"lcd_panel=Amp-WD\0" 			\
		"nfsroot=nfs_icore\0"			\
		"bootargs=set bootargs noinitrd console=ttymxc3,115200 arm_freq=800 engi_board=${board} video=${video_type},${lcd_panel}\0"		\
		"bootargs_net=setenv bootargs ${bootargs} root=/dev/nfs ip=dhcp nfsroot=${serverip}:${nfsroot},v3,tcp\0" 				\
		"bootargs_mmc=setenv bootargs ${bootargs} root=/dev/mmcblk0p1 rootwait rw\0" 								\
		"bootcmd_mmc=run bootargs; run bootargs_mmc; mmc dev 0; mmc read ${loadaddr} 0x800 0x2000;bootm\0" 					\
		"bootcmd_net=run bootargs; run bootargs_net; tftp uImage; bootm\0" \

	/* Customized parameter
	 * Customized parameter for SODIMM iCore modules
	 */
	#define	EXTRA_ENV_SETTINGS_ICORE 		\
		COMMON_PARAMETER 			\
		"bootcmd_ubi=run bootargs; run bootargs_ubi; nand read ${loadaddr} 0x400000 0x700000;bootm\0" 						\
		"bootargs_ubi=setenv bootargs ${bootargs} ubi.mtd=2 root=ubi0:rootfs rootfstype=ubifs rootwait rw\0"					\
		"video_type=mxcfb0:dev=lcd\0"		\
		"bootcmd=run bootcmd_ubi\0" 		\
		"erase=nand erase 1c0000 4000\0"	\
		"board=SK.RES\0"			\

	/* Customized parameter
	 * Customized parameter for QSEVEN RQS modules
	 */
	#define	EXTRA_ENV_SETTINGS_RQS 			\
		COMMON_PARAMETER 			\	
		"bootcmd_emmc=run bootargs; run bootargs_emmc; mmc dev 1; mmc read ${loadaddr} 0x800 0x2000;bootm\0"					\
		"bootargs_emmc=setenv bootargs ${bootargs} root=/dev/mmcblk0p1 rootwait rw\0" 								\
		"video_type=mxcfb0:dev=ldb,LDB-XGA,if=RGB666 video=mxcfb0:dev=hdmi,1920x1080M@60,if=RGB24\0"						\
		"bootcmd=run bootcmd_emmc\0" 		\
		"erase=mmc dev 1; mmc erase 400 1\0"	\
		"board=SK.Q7\0"				\


	// Additional defines
	#define _SERVER_IP_ADDR_	CONFIG_SERVERIP	


#endif //__COMMON_PARAMETER_H
