/*
 * user_main.c
 *
 *  ESP8266 based Sprinkler/Garden water controller
 *  Created on: June 1, 2015
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
#include "sntp.h"
#include "driver/uart.h"

#include "json.h"
#include "jsonparse.h"


#define GPIO_SET(gpio) \
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_OUT_W1TS_ADDRESS, 1<<gpio );
#define GPIO_CLR(gpio) \
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_OUT_W1TC_ADDRESS, 1<<gpio );

static os_timer_t startup_timer;
static os_timer_t delayed_start_timer;
static os_timer_t reset_timer;

#define SANE_TIME ( (time_t)1420070400 ) // 1/1/2015

#define CONFIG_ADDR 0x7c000
#define MAX_SCHEDS 30
#define CONFIG_MAGIC 40993519
struct sched_entry {
	int zone;
	time_t start;
	time_t end;
	time_t time;
	time_t duration;
	int repeat;
	int dow;
};
struct {
	int magic;
	char myname[50];
	char user[50];
	char pass[50];
	int zoffset;
	int sched_count;
	struct sched_entry scheds[MAX_SCHEDS];
} config;

struct time_entry {
	int zone;
	time_t ontime;
	int duration;
};
int time_count;
struct time_entry times[MAX_SCHEDS];

int countdown[8];

int t = 0;

#define SDATA 2
#define SCLK 5
#define SLAT 4

int relay_data = 0;


void ICACHE_FLASH_ATTR send_relay()
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

void ICACHE_FLASH_ATTR set_relay(int num, int onoff, int duration)
{
	if (num < 1 || num > 8) return;
	if (onoff)
	{
		os_printf("turning on %d for %d\n",num,duration);
		countdown[num-1] = duration;
		relay_data |= 1 << (num-1);
	}
	else
	{
		os_printf("turning off %d\n",num);
		countdown[num-1] = 0;
		relay_data &= ~(1 << (num-1));
	}
	send_relay();
}


int ICACHE_FLASH_ATTR get_relay(int num)
{
	if (num < 1 || num > 8) return 0;
	return ( relay_data & (1 << (num - 1 )) ) ? 1 : 0;
}
void ICACHE_FLASH_ATTR set_all_relays_off()
{
	int i;
	for (i = 1; i<=8; i++)
	{
		set_relay(i,0,0);
	}
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

	relay_data = 0;
	send_relay();

}

void ICACHE_FLASH_ATTR save_config()
{
	config.magic = CONFIG_MAGIC;
	spi_flash_erase_sector(CONFIG_ADDR / SPI_FLASH_SEC_SIZE);
	spi_flash_write(CONFIG_ADDR, (uint32*)&config, sizeof(config) );
}

void ICACHE_FLASH_ATTR load_config()
{
	os_printf("size of config=%d 0x%08x\n",sizeof(config), sizeof(config));
	spi_flash_read(CONFIG_ADDR, (uint32*)&config, sizeof(config));
	if (config.magic != CONFIG_MAGIC)
	{
		os_memset(&config,0,sizeof(config));
	}
	if (!config.myname[0])
	{
		uint8 mac[6];
		wifi_get_macaddr(STATION_IF, mac);
		os_sprintf(config.myname,"esp8266-%02x%02x%02x",mac[3],mac[4],mac[5]);
	}
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

void ICACHE_FLASH_ATTR do_reset0()
{
	system_restart();
}

void ICACHE_FLASH_ATTR do_reset1()
{
	os_printf("Closing down services, reset in 2 more seconds\n");
	set_all_relays_off();
	os_timer_disarm(&reset_timer);
	os_timer_setfn(&reset_timer, (os_timer_func_t *)do_reset0, (void *)0);
	os_timer_arm(&reset_timer, 2000, 1);

	espconn_mdns_close();
	sntp_stop();

	wifi_station_disconnect();

}

void ICACHE_FLASH_ATTR start_reset()
{
	os_printf("Starting reset in 2 seconds\n");
	os_timer_disarm(&delayed_start_timer);
	os_timer_disarm(&startup_timer);

	os_timer_disarm(&reset_timer);
	os_timer_setfn(&reset_timer, (os_timer_func_t *)do_reset1, (void *)0);
	os_timer_arm(&reset_timer, 2000, 1);

}

time_t ICACHE_FLASH_ATTR time()
{
	int t = sntp_get_current_timestamp();
	if (t < SANE_TIME) return 0;
	// they hard coded a GMT+8 timezone
	// we want to work with GMT all the way thru.
	t -= 8 * 60 * 60;
	return(t);
}

int ICACHE_FLASH_ATTR dow(time_t t)
{
	int dow = t + config.zoffset;
	dow = dow / 86400;
	dow = dow - 3;
	dow = dow % 7;
	return(1 << dow);  // day of week bitmask in proper timezone
}

time_t ICACHE_FLASH_ATTR midnight(time_t t)
{
	time_t mid = t + config.zoffset;
	mid /= 86400;
	mid *= 86400; // midnight current day proper timezone
	mid -= config.zoffset;  // local midnight in UTC
	return mid;
}

void compute_times()
{
	struct sched_entry *s;
	struct time_entry *t;
	time_count = 0;
	int i;
	time_t now = time();

	if (now < SANE_TIME) return;

	for (i = config.sched_count, s = config.scheds, t = times;
			i > 0;
			i--, s++)
	{
		time_t ontime = s->start;
		if (ontime == 0) ontime = now;
		ontime = midnight(ontime) + s->time;

		int failsafe = 0;
		while(ontime <= now || (dow(ontime) & s->dow) == 0 )
		{
			ontime += 86400 * s->repeat;
			failsafe++;
			if (failsafe > 100) break;
		}

		if (failsafe < 101)	if (ontime < s->end || s->end == 0)
		{
			t->ontime = ontime;
			t->duration = s->duration;
			t->zone = s->zone;
			os_printf("computed zone %d time %d (%d from now) duration %d\n",
					t->zone, t->ontime, (t->ontime - time()), t->duration);
			t++;
			time_count++;
		}
	}
}

#if 0
int __jsonparse_next(struct jsonparse_state *j)
{
	int type = jsonparse_next(j);
	char data[50] = "";
	if (type == 'N' || type == '0' || type == '"')
		jsonparse_copy_value(j, data, sizeof(data));
	os_printf("json type=%d %c -> %s\n",type,type,data);
	return(type);
}
#define jsonparse_next __jsonparse_next
#endif
int ICACHE_FLASH_ATTR cgiConfig(HttpdConnData *connData) {
	//os_printf("post len=%d %s\n",connData->post->len, connData->post->buff);
	char data[100];

	if (connData->post->len > 4)
	{
		struct jsonparse_state j;
		jsonparse_setup(&j, connData->post->buff, connData->post->len);
		int type;
		while ( (type = jsonparse_next(&j) ) != 0)
		{
			if (type == JSON_TYPE_PAIR_NAME) {
				if (jsonparse_strcmp_value(&j, "reset") == 0) {
					start_reset();
					os_sprintf(data,"{ \"reset\":\"Device reset in 2 seconds\" }");
					httpdSend(connData, data, -1);
					return HTTPD_CGI_DONE;
				}
				if (jsonparse_strcmp_value(&j, "config") == 0) {
					jsonparse_next(&j);
					while ( (type = jsonparse_next(&j) ) != 0)
					{
						if (type == JSON_TYPE_PAIR_NAME) {
							if (jsonparse_strcmp_value(&j, "name") == 0) {
								jsonparse_next(&j);
								jsonparse_next(&j);
								jsonparse_copy_value(&j, config.myname, sizeof(config.myname));
							}
							if (jsonparse_strcmp_value(&j, "zoffset") == 0) {
								jsonparse_next(&j);
								jsonparse_next(&j);
								// the following doesn't work with negative numbers
								//config.zoffset = jsonparse_get_value_as_int(&j);
								// so we do it the hard way
								char data[20];
								jsonparse_copy_value(&j, data, sizeof(data));
								// ok so atoi() doesn't work either
								config.zoffset = atoi(data);
#if 0
								if (data[0] == '-')
								{
									config.zoffset = 0 - atoi(data+1);
								}
								else
								{
									config.zoffset = atoi(data);
								}
#endif
								os_printf("zofffset=%d %08x\n",config.zoffset,config.zoffset);
							}
						}
					}
				}
			}
		}
		save_config();
	}
	os_sprintf(data,"{ config: { \"name\": \"%s\", \"zoffset\":%d } }",config.myname,config.zoffset);
	httpdSend(connData, data, -1);
	return HTTPD_CGI_DONE;
}
int ICACHE_FLASH_ATTR cgiSched(HttpdConnData *connData) {
	struct sched_entry *s = config.scheds;

	os_printf("post len=%d %s\n",connData->post->len, connData->post->buff);

	if (connData->post->len > 10)
	{

		set_all_relays_off();

		config.sched_count = 0;
		s = config.scheds;

		struct jsonparse_state j;
		jsonparse_setup(&j, connData->post->buff, connData->post->len);

		int type;
		while ( (type = jsonparse_next(&j) ) != 0)
		{
			if (type == JSON_TYPE_PAIR_NAME) {
				if (jsonparse_strcmp_value(&j, "schedules") == 0) {
					jsonparse_next(&j);
					while ( (type = jsonparse_next(&j) ) != 0)
					{
						if (type == JSON_TYPE_ARRAY || type == ',')
						{
							jsonparse_next(&j);
							while ( (type = jsonparse_next(&j) ) != 0)
							{
								if (type == '}')
								{
									s++;
									config.sched_count++;
									break;
								}
								if (type == JSON_TYPE_PAIR_NAME) {
									if (jsonparse_strcmp_value(&j, "zone") == 0) {
										jsonparse_next(&j);
										jsonparse_next(&j);
										s->zone = jsonparse_get_value_as_int(&j);
									}
									if (jsonparse_strcmp_value(&j, "start") == 0) {
										jsonparse_next(&j);
										jsonparse_next(&j);
										s->start = jsonparse_get_value_as_int(&j);
									}
									if (jsonparse_strcmp_value(&j, "end") == 0) {
										jsonparse_next(&j);
										jsonparse_next(&j);
										s->end = jsonparse_get_value_as_int(&j);
									}
									if (jsonparse_strcmp_value(&j, "time") == 0) {
										jsonparse_next(&j);
										jsonparse_next(&j);
										s->time = jsonparse_get_value_as_int(&j);
									}
									if (jsonparse_strcmp_value(&j, "duration") == 0) {
										jsonparse_next(&j);
										jsonparse_next(&j);
										s->duration = jsonparse_get_value_as_int(&j);
									}
									if (jsonparse_strcmp_value(&j, "repeat") == 0) {
										jsonparse_next(&j);
										jsonparse_next(&j);
										s->repeat = jsonparse_get_value_as_int(&j);
									}
									if (jsonparse_strcmp_value(&j, "dow") == 0) {
										jsonparse_next(&j);
										jsonparse_next(&j);
										s->dow = jsonparse_get_value_as_int(&j);
									}
								}
							}
						}
					}
				}
			}
		}
		save_config();
		compute_times();
	}

	char *data = (char *)alloca(100*config.sched_count);
	char *p = data;
	s = config.scheds;
	int i;
	os_sprintf(p,"{ \"name\": \"%s\", \"time\": %d, \"schedules\":[ ",config.myname,time());
	p += strlen(p);
	for (i = config.sched_count, s = config.scheds; i > 0; i--, s++)
	{
		os_sprintf(p,"{\"zone\":%d,\"start\":%d,\"end\":%d,\"time\":%d,\"duration\":%d,\"repeat\":%d,\"dow\":%d},",
				s->zone,s->start,s->end,s->time,s->duration,s->repeat,s->dow);
		p += strlen(p);
	}
	p--;
	*p++ = ']';
	*p++ = '}';
	*p++ = '\0';
	httpdSend(connData, data, -1);
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiStatus(HttpdConnData *connData) {
	char buff[50];
	char buff2[10];
	int len=httpdFindArg(connData->post->buff, "chan", buff, sizeof(buff));
	int len2=httpdFindArg(connData->post->buff, "val", buff2, sizeof(buff2));
	if (len > 0 && len2 > 0)
	{
		os_printf("chan=%s val=%s\n",buff,buff2);
		int relay = atoi(buff);
		int onoff = atoi(buff2);
		set_relay(relay,onoff,300);
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

static long hitCounter=0;

//Template code for the counter on the index page.
int ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[20] = "";
	if (token==NULL) return HTTPD_CGI_DONE;

	if (os_strcmp(token, "counter")==0) {
		hitCounter++;
		os_sprintf(buff, "%ld", hitCounter);
	}
	if (os_strcmp(token, "myname")==0) {
		hitCounter++;
		os_strcpy(buff,config.myname);
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
	{"/index.tpl", cgiEspFsTemplate, tplCounter},
	{"/config.cgi", cgiConfig, NULL},
	{"/status.cgi", cgiStatus, NULL},
	{"/sched.cgi", cgiSched, NULL},
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
	int i;
	struct time_entry *t;
	int recompute = 0;
	static int first_compute = 0;
	if (!first_compute)
	{
		if (time() > SANE_TIME)
		{
			first_compute = 1;
			compute_times();
		}
		os_printf("waiting for sane_time %d %d\n",sntp_get_current_timestamp(),time());
		return;
	}
	for (i = 1; i <=8; i++)
	{
		if (countdown[i-1])
		{
			countdown[i-1]--;
			if (countdown[i-1] == 0)
			{
				set_relay(i,0,0);
			}
		}
	}

	time_t now = time();
	for (i = time_count, t = times; i > 0; i--, t++)
	{
		//os_printf("zone=%d now=%d ontime=%d recompute=%d ctr=%d\n",t->zone,now, t->ontime, recompute, i);
		if (now >= t->ontime)
		{
			recompute++;

			if (t->zone == 100)
			{
				start_reset();
			}
			else if (t->zone == 101)
			{
				set_all_relays_off();
			}
			else
			{
				set_relay(t->zone,1,t->duration);
			}
		}
	}

	if (recompute) compute_times();

}

static struct mdns_info m;

void minit(void)
{
	struct ip_info ipconfig;
    wifi_get_ip_info(STATION_IF, &ipconfig);

	m.host_name = config.myname;
	m.server_name = "sprinkler";
	m.server_port = 80;
	m.ipAddr = ipconfig.ip.addr;
	//m.txt_data[0] = "test=test";
	espconn_mdns_init(&m);
}

void ntp_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{

	sntp_setserver(0, ipaddr);
}


void ninit(void)
{
	sntp_init();
	ip_addr_t ip;
	int r = dns_gethostbyname("pool.ntp.org", &ip, ntp_dns_found, NULL);
    if (r == ESPCONN_OK)
    {
    	sntp_setserver(0, &ip);
    }

}


void delayed_start(void *arg)
{
	if (wifi_station_get_connect_status() != STATION_GOT_IP) return;

	os_timer_disarm(&delayed_start_timer);
    os_printf("wifi connected in client mode, starting mdns and ntp.\n");
    minit();
    ninit();
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
	os_timer_arm(&startup_timer, 1000, 1);

	os_timer_disarm(&delayed_start_timer);
	os_timer_setfn(&delayed_start_timer, (os_timer_func_t *)delayed_start, (void *)0);
	os_timer_arm(&delayed_start_timer, 500, 1);

	load_config();

	os_printf("running\n");

}

void ICACHE_FLASH_ATTR user_rf_pre_init()
{

}
