#include "main.h"
#include "stm32f10x.h"

#include "ch.h"
#include "hal.h"

#include "enc.h"
#include "uip.h"
#include "uip_arp.h"

#include "evtimer.h"

// Указатель на поток
Thread *tp = NULL;
Thread *tp2 = NULL;

// Структура связи с ENC60J28
struct enc encST = {
	&SPID1,
	{
		NULL,
		GPIOA,
		GPIOA_ENC_CS,
		SPI_CR1_BR_0 | SPI_CR1_BR_1
	},
	(uint8_t) 0,
	(uint16_t) 0,
	{
	ETHADDR0, ETHADDR1, ETHADDR2, ETHADDR3, ETHADDR4, ETHADDR5
	}
};

static const struct uip_eth_addr macaddr = {
	{ETHADDR0, ETHADDR1, ETHADDR2, ETHADDR3, ETHADDR4, ETHADDR5}
};

static EVENTSOURCE_DECL(enc_int_es);

static void enc_int(EXTDriver *drv, expchannel_t c)
{
	(void)drv;
	(void)c;
	chEvtBroadcast(&enc_int_es);
}

static const EXTConfig extcfg = {
	{
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_FALLING_EDGE | EXT_MODE_GPIOA | EXT_CH_MODE_AUTOSTART, enc_int},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL},
		{EXT_CH_MODE_DISABLED, NULL}
	}
};

#define EVID_NET_INT			0
#define EVID_NET_PERIODIC		1
#define EVID_NET_ARP			2

static void network_send()
{
	if (uip_len <= UIP_LLH_LEN + 40)
		enc_packet_send(&encST, uip_len, uip_buf, 0, NULL);
	else
		enc_packet_send(&encST, 54, uip_buf,
				uip_len - UIP_LLH_LEN - 40, uip_appdata);
}
// -----------------------------------------------------------
// СЕТЬ
// -----------------------------------------------------------
static WORKING_AREA(network_wa, 512);
static msg_t network_thread(void *arg)
{
	uip_ipaddr_t ip = {0 , 0};
	EvTimer per_evt;
	EvTimer arp_evt;
	EventListener per_el;
	EventListener arp_el;
	EventListener int_el;
	
	eventmask_t event;
	int i;
	uint8_t flags;
	uint8_t count = 0;

	(void)arg;
	
	tp2 = chThdSelf();	

	evtInit(&per_evt, MS2ST(1000));
	evtInit(&arp_evt, S2ST(10));

	chEvtRegister(&per_evt.et_es, &per_el, EVID_NET_PERIODIC);
	chEvtRegister(&arp_evt.et_es, &arp_el, EVID_NET_ARP);
	chEvtRegister(&enc_int_es, &int_el, EVID_NET_INT);

	spiStart(&SPID1, &encST.config);

	enc_init(&encST);
	uip_init();
	uip_setethaddr(macaddr);
	//uip_ipaddr(ip, IPADDR0, IPADDR1, IPADDR2, IPADDR3);
	//uip_sethostaddr(ip);
	//-----------------------
	uip_ipaddr(ip, 192,168,1,111);
	uip_sethostaddr(ip);
	uip_ipaddr(ip, 192,168,1,100);
	uip_setdraddr(ip);
	uip_ipaddr(ip, 255,255,255,0);
	uip_setnetmask(ip);
	//-----------------------
	uip_listen(HTONS(80));
	
	evtStart(&per_evt);
	evtStart(&arp_evt);

	while (1)
	{
		if (!palReadPad(GPIOA, GPIOA_ENC_INT))
		{
			event = EVENT_MASK(EVID_NET_INT);
		}
		else
		{
			chSysLock();
				palClearPad(GPIOB, GPIOB_PIN0);
				event = chEvtWaitAny(ALL_EVENTS);
				palSetPad(GPIOB, GPIOB_PIN0);
			chSysUnlock();
		}
		
		if (event == EVENT_MASK(EVID_NET_INT))
		{
			/* The interrupt line is low for as long as we have packets pending */
			flags = enc_int_flags(&encST);
			if (flags & ENC_PKTIF)
			{
				while ((uip_len = enc_packet_receive(&encST, UIP_BUFSIZE, uip_buf)) != 0)
				{
					switch(ntohs((((struct uip_eth_hdr *)&uip_buf[0])->type)))
					{
					case UIP_ETHTYPE_IP:
						uip_arp_ipin();
						uip_input();
						if (uip_len > 0)
						{
							uip_arp_out();
							network_send();
						}
						break;
					case UIP_ETHTYPE_ARP:
						uip_arp_arpin();
						if (uip_len > 0)
						{
							network_send();
						}
						break;
					default: /* This usually happens on overflows, hackish but works */
						enc_init(&encST);
						chSysLock();
							palTogglePad(GPIOB, GPIOB_PIN1);
						chSysUnlock();
						break;
					}
				}
			}
			enc_int_clear(&encST, ENC_ALLIF);
			/* TODO: This patches the race condition, fix... */
			chThdSleepMicroseconds(100);
		}
		else if (event == EVENT_MASK(EVID_NET_PERIODIC))
		{
			for (i = 0; i < UIP_CONNS; i++)
			{
				uip_periodic(i);
				if (uip_len > 0)
				{
					uip_arp_out();
					network_send();
				}
			}
		}
		else
		{
			uip_arp_timer();
		}
		
		count++;
		if (count == 10)
		{
			count = 0;
			chSysLock();
			palTogglePad(GPIOB, GPIOB_PIN2);
			chSysUnlock();
		}
	}
}
// -----------------------------------------------------------
// МИГАЛКА
// -----------------------------------------------------------
static WORKING_AREA(waThreadBlink, 128);
static msg_t ThreadBlink(void *arg)
{
	while (1)
	{
		chSysLock();
		palTogglePad(GPIOB, GPIOB_PIN7);
		
		if (palReadPad(GPIOA, GPIOB_PIN15) == 1)
		{
			
				palTogglePad(GPIOB, GPIOB_PIN5);
				spiStop(&SPID1);
				spiStart(&SPID1, &encST.config);
				enc_init(&encST);
		}
		chSysUnlock();
		
		chThdSleepMilliseconds(200);
	}
}
// -----------------------------------------------------------
int main(void)
{
	halInit();
	chSysInit();

	spiInit();
	extInit();

	chEvtInit(&enc_int_es);
	extStart(&EXTD1, &extcfg);

	chThdCreateStatic(network_wa, sizeof(network_wa), NORMALPRIO, network_thread, NULL);
	chThdCreateStatic(waThreadBlink, sizeof(waThreadBlink), NORMALPRIO, ThreadBlink, NULL);

	while (1)
	{
		chThdSleepMilliseconds(500);
	}
}
