/*
 * user_main.c
 *
 * Test/example program for the Simple mDNS responder
 *
 *  Simple mDNS responder.
 *  It replys to mDNS IP (IPv4/A) queries and optionally broadcasts ip advertisements
 *  using the mDNS protocol
 *  Created on: Apr 10, 2015
 *      Author: Kevin Uhlir (n0bel)
 *
 */


#include <esp8266.h>
#include "httpd.h"
#include "httpdespfs.h"
#include "cgiwifi.h"
#include "cgiflash.h"
#include "auth.h"
#include "espfs.h"

#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include "driver/uart.h"
#include "user_interface.h"
#include "mem.h"

#define GPIO_SET(gpio) \
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_OUT_W1TS_ADDRESS, 1<<gpio );
#define GPIO_CLR(gpio) \
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_OUT_W1TC_ADDRESS, 1<<gpio );

extern int ets_uart_printf(const char *fmt, ...);
static os_timer_t startup_timer;

int t = 0;

#define SDATA 2
#define SCLK 5
#define SLAT 4

int relay_data = 0;

void set_relay(int num, int onoff)
{
	if (num < 1 || num > 8) return;
	if (onoff)
		relay_data |= 1 << (num-1);
	else
		relay_data &= ~(1 << (num-1));
}

int get_relay(int num)
{
	if (num < 1 || num > 8) return 0;
	return ( relay_data & (1 << (num - 1 )) ) ? 1 : 0;
}

void send_relay()
{
	int t = relay_data;
	// ets_uart_printf("output %d\n",t);

	t&128 ? GPIO_OUTPUT_SET(SDATA,(1)) : GPIO_OUTPUT_SET(SDATA,(0)); GPIO_OUTPUT_SET(SCLK,1); GPIO_OUTPUT_SET(SCLK,0);
	t&64 ? GPIO_OUTPUT_SET(SDATA,(1)) : GPIO_OUTPUT_SET(SDATA,(0)); GPIO_OUTPUT_SET(SCLK,1); GPIO_OUTPUT_SET(SCLK,0);
	t&32 ? GPIO_OUTPUT_SET(SDATA,(1)) : GPIO_OUTPUT_SET(SDATA,(0)); GPIO_OUTPUT_SET(SCLK,1); GPIO_OUTPUT_SET(SCLK,0);
	t&16 ? GPIO_OUTPUT_SET(SDATA,(1)) : GPIO_OUTPUT_SET(SDATA,(0)); GPIO_OUTPUT_SET(SCLK,1); GPIO_OUTPUT_SET(SCLK,0);
	t&8 ? GPIO_OUTPUT_SET(SDATA,(1)) : GPIO_OUTPUT_SET(SDATA,(0)); GPIO_OUTPUT_SET(SCLK,1); GPIO_OUTPUT_SET(SCLK,0);
	t&4 ? GPIO_OUTPUT_SET(SDATA,(1)) : GPIO_OUTPUT_SET(SDATA,(0)); GPIO_OUTPUT_SET(SCLK,1); GPIO_OUTPUT_SET(SCLK,0);
	t&2 ? GPIO_OUTPUT_SET(SDATA,(1)) : GPIO_OUTPUT_SET(SDATA,(0)); GPIO_OUTPUT_SET(SCLK,1); GPIO_OUTPUT_SET(SCLK,0);
	t&1 ? GPIO_OUTPUT_SET(SDATA,(1)) : GPIO_OUTPUT_SET(SDATA,(0)); GPIO_OUTPUT_SET(SCLK,1); GPIO_OUTPUT_SET(SCLK,0);
	GPIO_OUTPUT_SET(SLAT,1); GPIO_OUTPUT_SET(SLAT,0);
}

void init_relay()
{
	// make sure GPIOs enabled to GPIO function not alternate function

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

	GPIO_OUTPUT_SET(SDATA, 0);
	GPIO_OUTPUT_SET(SCLK, 0);
	GPIO_OUTPUT_SET(SLAT, 0);

	send_relay();

}


//#define SHOW_HEAP_USE

//Function that tells the authentication system what users/passwords live on the system.
//This is disabled in the default build; if you want to try it, enable the authBasic line in
//the builtInUrls below.
int myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
	if (no==0) {
		os_strcpy(user, "admin");
		os_strcpy(pass, "s3cr3t");
		return 1;
//Add more users this way. Check against incrementing no for each user added.
//	} else if (no==1) {
//		os_strcpy(user, "user1");
//		os_strcpy(pass, "something");
//		return 1;
	}
	return 0;
}

int ICACHE_FLASH_ATTR cgiJson(HttpdConnData *connData) {
	char buff[50];
	char buff2[10];
	int len=httpdFindArg(connData->post->buff, "chan", buff, sizeof(buff));
	int len2=httpdFindArg(connData->post->buff, "val", buff2, sizeof(buff2));
	os_printf("chan=%s val=%s\n",buff,buff2);
	if (len > 0 && len2 > 0)
	{
		int relay = atoi(buff);
		int onoff = atoi(buff2);
		set_relay(relay,onoff);
		send_relay();
	}

	char *sp = buff;
	*sp++ = '[';
	int i;
	for (i = 1; i <= 8; i++)
	{
		*sp++ = '"';
		*sp++ = 'o';
		if (get_relay(i))
		{
			*sp++ = 'n';
		}
		else
		{
			*sp++ = 'f';
			*sp++ = 'f';
		}
		*sp++ = '"';
		*sp++ = ',';
	}
	sp--;
	*sp++ = ']';
	*sp++ = '\0';
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiRelay(HttpdConnData *connData) {
	int len;
	int relay;
	int onoff;
	char buff[10];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->post->buff, "relay1", buff, sizeof(buff));
	if (len>0) {
		onoff = buff[1] == 'n';
		set_relay(1,onoff);
	}
	len=httpdFindArg(connData->post->buff, "relay2", buff, sizeof(buff));
	if (len>0) {
		onoff = buff[1] == 'n';
		set_relay(2,onoff);
	}
	len=httpdFindArg(connData->post->buff, "relay3", buff, sizeof(buff));
	if (len>0) {
		onoff = buff[1] == 'n';
		set_relay(3,onoff);
	}
	len=httpdFindArg(connData->post->buff, "relay4", buff, sizeof(buff));
	if (len>0) {
		onoff = buff[1] == 'n';
		set_relay(4,onoff);
	}
	len=httpdFindArg(connData->post->buff, "relay5", buff, sizeof(buff));
	if (len>0) {
		onoff = buff[1] == 'n';
		set_relay(5,onoff);
	}
	len=httpdFindArg(connData->post->buff, "relay6", buff, sizeof(buff));
	if (len>0) {
		onoff = buff[1] == 'n';
		set_relay(6,onoff);
	}
	len=httpdFindArg(connData->post->buff, "relay7", buff, sizeof(buff));
	if (len>0) {
		onoff = buff[1] == 'n';
		set_relay(7,onoff);
	}
	len=httpdFindArg(connData->post->buff, "relay8", buff, sizeof(buff));
	if (len>0) {
		onoff = buff[1] == 'n';
		set_relay(8,onoff);
	}
	send_relay();

	httpdRedirect(connData, "relay.tpl");
	return HTTPD_CGI_DONE;
}



//Template code for the led page.
int ICACHE_FLASH_ATTR tplRelay(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	os_strcpy(buff, "Unknown");
	if (os_strncmp(token, "relay_state_",12)==0) {
		int relay = atoi(token+12);
		if (get_relay(relay)) {
			os_strcpy(buff, "on");
		} else {
			os_strcpy(buff, "off");
		}
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

static long hitCounter=0;

//Template code for the counter on the index page.
int ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	if (os_strcmp(token, "counter")==0) {
		hitCounter++;
		os_sprintf(buff, "%ld", hitCounter);
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}



/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/index.tpl"},
	{"/flash.bin", cgiReadFlash, NULL},
	{"/relay.tpl", cgiEspFsTemplate, tplRelay},
	{"/index.tpl", cgiEspFsTemplate, tplCounter},
	{"/relay.cgi", cgiRelay, NULL},
	{"/json.cgi", cgiJson, NULL},
	{"/updateweb.cgi", cgiUploadEspfs, NULL},

	//Routines to make the /wifi URL and everything beneath it work.

//Enable the line below to protect the WiFi configuration with an username/password combo.
//	{"/wifi/*", authBasic, myPassFn},

	{"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/connstatus.cgi", cgiWiFiConnStatus, NULL},
	{"/wifi/setmode.cgi", cgiWiFiSetMode, NULL},

	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};


#ifdef SHOW_HEAP_USE
static ETSTimer prHeapTimer;

static void ICACHE_FLASH_ATTR prHeapTimerCb(void *arg) {
	os_printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
}
#endif


void ICACHE_FLASH_ATTR onesec(void *arg)
{
//	set_relay(t,0);
//	t++;
//	if (t > 8) t = 1;
//	ets_uart_printf("setting relay %d\n",t);
//	set_relay(t,1);
//	send_relay();
}


void ICACHE_FLASH_ATTR user_init(void)
{
	uart0_init(BIT_RATE_74880);

	// 0x40200000 is the base address for spi flash memory mapping, ESPFS_POS is the position
	// where image is written in flash that is defined in Makefile.
	espFsInit((void*)(0x40200000 + ESPFS_POS));
	httpdInit(builtInUrls, 80);
#ifdef SHOW_HEAP_USE
	os_timer_disarm(&prHeapTimer);
	os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
	os_timer_arm(&prHeapTimer, 3000, 1);
#endif

	init_relay();

	os_timer_disarm(&startup_timer);
	os_timer_setfn(&startup_timer, (os_timer_func_t *)onesec, (void *)0);
	os_timer_arm(&startup_timer, 3000, 1);
	os_printf("running\n");

}

void ICACHE_FLASH_ATTR user_rf_pre_init()
{

}
