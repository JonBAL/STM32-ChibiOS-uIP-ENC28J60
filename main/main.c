#include "main.h"
#include "stm32f10x.h"

#include "ch.h"
#include "hal.h"

#include "enc.h"
#include "uip.h"
#include "uip_arp.h"

#include "evtimer.h"

// uip-conf.h #define UIP_CONF_BUFFER_SIZE     1518

// Указатель на поток
Thread *tp = NULL;
Thread *tp2 = NULL;

// Буферы
//static uint8_t rxbuf[8];
//static uint8_t txbuf[8];

// Виртуальные таймеры
//static VirtualTimer vt1;

//static void rxend(UARTDriver *uartp);
//	
//// Структура конфигурации UART-драйвера
//static UARTConfig uart_cfg_1 = {
//  NULL,
//  NULL,
//  rxend,
//  NULL,
//  NULL,
//  9600,
//  0,
//  USART_CR2_LINEN,
//  0
//};

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

//static WORKING_AREA(wa_spi_thread, 256);
//static msg_t spi_thread(void *p)
//{
//	//uip_ipaddr_t ip = {0 , 0};
//	chRegSetThreadName("SPI thread");
//  while (1)
//	{
//		// Перевести поток в состояние сна (приостановка выполнения потока)
//		chSysLock();
//			tp = chThdSelf();
//			chSchGoSleepS(THD_STATE_SUSPENDED);
//		chSysUnlock();

//		palTogglePad(GPIOB, GPIOB_PIN4);

//		//txbuf[0] = enc_register_read(&encST, ESTAT);
//		//enc_register_write(&encST, ECON1, 0x01);
//		//txbuf[1] = enc_register_read(&encST, ECON1);
//		//enc_register_write(&encST, ECON1, 0x05);
//		//txbuf[2] = enc_register_read(&encST, ECON2);
//		
////		chSysLock();
////			enc_init(&encST);
////			uip_init();
////			uip_setethaddr(macaddr);
////			//-----------------------
////			uip_ipaddr(ip, 192,168,1,111);
////			uip_sethostaddr(ip);
////			uip_ipaddr(ip, 192,168,1,100);
////			uip_setdraddr(ip);
////			uip_ipaddr(ip, 255,255,255,0);
////			uip_setnetmask(ip);
////			//-----------------------
////			uip_listen(HTONS(80));
////		chSysUnlock();

//		// Отправка заполненного буфера обратно
//		uartStartSendI(&UARTD1, 3, &txbuf);
//		
//		// Разрешение приёма (если не разрешить, rxend перестанет вызываться и начнёт работать rxchar)
//		uartStartReceiveI(&UARTD1, 1, &rxbuf);
//  }
//}

//// Эта функция вызывается когда буфер приёмника заполнен
//static void rxend(UARTDriver *uartp)
//{
//	Thread *ntp = tp;
//	chSysLock();
//		if (ntp)
//		{ 
//			tp = NULL;
//			chSchWakeupS(ntp, RDY_OK);
//		}
//	chSysUnlock(); 
//}

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
#define EVID_NET_PERIODIC	1
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
	//uip_ipaddr_t ip;
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
		
		//chSysLock();
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
		//chSysUnlock();
		
		count++;
		if (count == 10)
		{
			count = 0;
			chSysLock();
			palTogglePad(GPIOB, GPIOB_PIN2);
			chSysUnlock();
		}
		
//		if ((encST.driver->state) == 1)
//		{
//			palSetPad(GPIOB, GPIOB_PIN5);
//			palSetPad(GPIOB, GPIOB_PIN4);
//		}
//		else
//		{
//			palClearPad(GPIOB, GPIOB_PIN5);
//			palClearPad(GPIOB, GPIOB_PIN4);
//		}
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
//			chThdWait(tp2);
//			chThdTerminate (tp2);
//			//chThdSleepMilliseconds(100);
//			
//			chThdCreateStatic(network_wa, sizeof(network_wa), NORMALPRIO, network_thread, NULL);
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
	
//	uartStart(&UARTD1, &uart_cfg_1);
//  uartStartSend(&UARTD1, 13, "Starting...\r\n");
	
//	// Создание потока связи с SPI
//	chThdCreateStatic(wa_spi_thread, sizeof(wa_spi_thread), NORMALPRIO, spi_thread, NULL);
	// Создание потока для сети
	chThdCreateStatic(network_wa, sizeof(network_wa), NORMALPRIO, network_thread, NULL);
	// Создание потока мигалки
	chThdCreateStatic(waThreadBlink, sizeof(waThreadBlink), NORMALPRIO, ThreadBlink, NULL);
	//
	//chThdCreateStatic(wa_enc_reset_thread, sizeof(wa_enc_reset_thread), NORMALPRIO, enc_reset_thread, NULL);
	
//	// Разрешение приёма по UART2
//	uartStartReceive(&UARTD1, 1, &rxbuf);

	//chThdExit(0);
	
	while (1)
	{
		chThdSleepMilliseconds(500);
  }
}
