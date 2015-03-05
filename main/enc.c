#include "hal.h"
#include "enc.h"
	
#define enc_spi_start(enc) spiSelect((enc)->driver);
#define enc_spi_stop(enc) spiUnselect((enc)->driver);

void enc_reset(struct enc *enc)
{
	uint8_t d = INST_RESET;
	
	// Reset ENC-chip
	chSysLock();
	palClearPad(GPIOA, GPIOA_ENC_RST);
	//chThdSleepMilliseconds(1);
	chThdSleepMicroseconds(100);
	palSetPad(GPIOA, GPIOA_ENC_RST);
	chSysUnlock();

	enc_spi_start(enc);
	spiSend(enc->driver, 1, &d);
	enc_spi_stop(enc);
	
	// For fix ERRATA: 2. Module: Reset
	chThdSleepMilliseconds(1);
}

static void enc_write_data(struct enc *enc, int n, uint8_t *data)
{
	enc_spi_start(enc);
	spiSend(enc->driver, n, data);
	enc_spi_stop(enc);
}

void enc_write(struct enc *e, int n, uint8_t mask1, uint8_t mask2)
{
	uint8_t data[2];
	data[0] = mask1;
	data[1] = mask2;
	enc_write_data(e, n, (uint8_t *)data);
}
	
static void enc_read(struct enc *enc, uint8_t inst, int n, uint8_t *ret)
{
	enc_spi_start(enc);
	spiSend(enc->driver, 1, &inst);
	spiReceive(enc->driver, n, ret);
	enc_spi_stop(enc);
}

void enc_bit_set(struct enc *enc, uint8_t addr, uint8_t mask)
{
	enc_write(enc, 2, INST_BIT_SET | (ADDR_MASK & addr), mask);
}

void enc_bit_clr(struct enc *enc, uint8_t addr, uint8_t mask)
{
	enc_write(enc, 2, INST_BIT_CLEAR | (ADDR_MASK & addr), mask);
}

static void enc_set_bank(struct enc *enc, uint8_t bank)
{
	bank = BANK_NUM(bank);
	
	if (enc->bank == bank) return;
	
	enc_bit_clr(enc, ECON1, ECON1_BSEL0 | ECON1_BSEL1);
	enc_bit_set(enc, ECON1, bank);
	enc->bank = bank;
}

static void enc_register_write(struct enc *enc, uint8_t addr, uint8_t val)
{ // was static
	enc_set_bank(enc, addr);
	enc_write(enc, 2, INST_CONTROL_WRITE | (ADDR_MASK & addr), val);
}

static uint8_t enc_register_read(struct enc *enc, uint8_t addr)
{	// was static
	uint8_t ret[2] = { 0, 0 };
	bool dummy = NEEDS_DUMMY(addr);
	
	enc_set_bank(enc, addr);
	enc_read(enc, INST_CONTROL_READ | (ADDR_MASK & addr), dummy ? 2 : 1, ret);
	
	return ret[dummy ? 1 : 0];
}

static void enc_buffer_write(struct enc *enc, int len, uint8_t *data)
{
	uint8_t inst[1] = {INST_BUFFER_WRITE};
	
	enc_spi_start(enc);
	spiSend(enc->driver, 1, inst);
	spiSend(enc->driver, len, data);
	enc_spi_stop(enc);
}

#define enc_buffer_read(e, l, d) enc_read(e, INST_BUFFER_READ, l, d);

static void enc_phy_write(struct enc *enc, uint8_t addr, uint16_t data)
{
	enc_register_write(enc, MIREGADR, addr);
	enc_register_write(enc, MIWRL, data);
	enc_register_write(enc, MIWRH, data >> 8);

	while(enc_register_read(enc, MISTAT) & MISTAT_BUSY)
		chThdSleepMicroseconds(20);
}

void enc_int_clear(struct enc *enc, uint8_t mask)
{
	enc_bit_clr(enc, EIR, mask);
}

uint8_t enc_int_flags(struct enc *enc)
{
	return enc_register_read(enc, EIR);
}

// For debug
uint8_t enc_read_REG(struct enc *enc, uint8_t reg)
{
	return enc_register_read(enc, reg);
}

void enc_packet_send(struct enc *enc, uint16_t len1, uint8_t *data1, uint16_t len2, uint8_t *data2)
{
	uint8_t data[] = {0};
	uint16_t i = 0xFFFF;
	
	while(enc_register_read(enc, ECON1) & ECON1_TXRTS)
	{
//		if(enc_register_read(enc, EIR) & EIR_TXERIF)
//		{
//			enc_bit_set(enc, ECON1, ECON1_TXRST);
//			enc_bit_clr(enc, ECON1, ECON1_TXRST);
//			break;
//		}
		
		i--;
		if (i == 0) break; 
	}
	
	enc_bit_set(enc, ECON1, ECON1_TXRST);
	enc_bit_clr(enc, ECON1, ECON1_TXRST);

	/* Set pointer to start of the byte */
	enc_register_write(enc, EWRPTL, TXSTART_INIT & 0xff);
	enc_register_write(enc, EWRPTH, TXSTART_INIT >> 8);
	/* Set end of the buffer to the end of the packet */
	enc_register_write(enc, ETXNDL, (TXSTART_INIT + len1 + len2) & 0xff);
	enc_register_write(enc, ETXNDH, (TXSTART_INIT + len1 + len2) >> 8);
	
	/* Per packet config byte, use defaults */
	enc_buffer_write(enc, 1, data);
	enc_buffer_write(enc, len1, data1);
	
	if (len2 > 0) enc_buffer_write(enc, len2, data2);

	enc_bit_set(enc, ECON1, ECON1_TXRTS);
}

uint16_t enc_packet_receive(struct enc *enc, uint16_t maxlen, uint8_t *data)
{
	uint16_t stat;
	uint16_t len;

	// If Ethernet Packet Count = 0, exit
	if (enc_register_read(enc, EPKTCNT) == 0)	return 0;

	/* Set the read pointer */
	enc_register_write(enc, ERDPTL, enc->ptr & 0xff);
	enc_register_write(enc, ERDPTH, enc->ptr >> 8);

	/* Read the next packet pointer (This works only on little-endian) */
	enc_buffer_read(enc, 2, (uint8_t *) &enc->ptr);

	/* Read packet length */
	enc_buffer_read(enc, 2, (uint8_t *) &len);
	// Cut CRC
	len -= 4;

	/* Read the rest of the status bits */
	enc_buffer_read(enc, 2, (uint8_t *) &stat);

	if (len > maxlen)	len = maxlen;

	enc_buffer_read(enc, len, data);

	enc_register_write(enc, ERXRDPTL, (enc->ptr - 1) & 0xff);
	enc_register_write(enc, ERXRDPTH, enc->ptr >> 8);

	/* Decrement the EPKTCNT */
	enc_bit_set(enc, ECON2, ECON2_PKTDEC);

	return len;
}

void enc_drop_packets(struct enc *enc)
{
	/* Set the pointer to the start of the buffer */
	enc->ptr = RXSTART_INIT;
	
	enc_register_write(enc, ERXRDPTL, RXSTART_INIT & 0xff);
	enc_register_write(enc, ERXRDPTH, RXSTART_INIT >> 8);
	/* Set the packet counter to 0 */
	enc_register_write(enc, EPKTCNT, 0);
}

void enc_init(struct enc *enc)
{
	enc_reset(enc);
	//chThdSleepMilliseconds(100); //100
	
	enc->ptr = RXSTART_INIT;
	/* RX buffer start */
	enc_register_write(enc, ERXSTL, RXSTART_INIT & 0xff);
	enc_register_write(enc, ERXSTH, RXSTART_INIT >> 8);
	/* RX buffer pointer */
	enc_register_write(enc, ERXRDPTL, RXSTART_INIT & 0xff);
	enc_register_write(enc, ERXRDPTH, RXSTART_INIT >> 8);
	/* RX buffer end */
	enc_register_write(enc, ERXNDL, RXSTOP_INIT & 0xff);
	enc_register_write(enc, ERXNDH, RXSTOP_INIT >> 8);

	/* TX buffer start */
	enc_register_write(enc, ETXSTL, TXSTART_INIT & 0xff);
	enc_register_write(enc, ETXSTH, TXSTART_INIT >> 8);
	/* TX buffer end */
	enc_register_write(enc, ETXNDL, TXSTOP_INIT & 0xff);
	enc_register_write(enc, ETXNDH, TXSTOP_INIT >> 8);

	/* Drop packets with invalid CRC */
	enc_register_write(enc, ERXFCON, ERXFCON_CRCEN);

	/* Enable rx/tx of pause frames and enables RX */
	enc_register_write(enc, MACON1, MACON1_TXPAUS | MACON1_RXPAUS | MACON1_MARXEN);
	enc_register_write(enc, MACON2, 0);
	/* Pad to 60 bytes and append CRC,  */
	enc_register_write(enc, MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN);
	/* Set inter frame gaps */
	enc_register_write(enc, MAIPGL, 0x12);
	enc_register_write(enc, MAIPGH, 0x0c);
	enc_register_write(enc, MABBIPG, 0x12);

	/* MAC is reversed */
	enc_register_write(enc, MAADR5, enc->mac[0]);
	enc_register_write(enc, MAADR4, enc->mac[1]);
	enc_register_write(enc, MAADR3, enc->mac[2]);
	enc_register_write(enc, MAADR2, enc->mac[3]);
	enc_register_write(enc, MAADR1, enc->mac[4]);
	enc_register_write(enc, MAADR0, enc->mac[5]);

	/* Disable loopback */
	enc_phy_write(enc, PHCON2, PHCON2_HDLDIS);
	/* Enable interrupts */
	enc_register_write(enc, EIE, EIE_INTIE | EIE_PKTIE | EIE_RXERIE);

	/* Set bank to 0 and enables RX */
	enc_register_write(enc, ECON1, ECON1_RXEN);
	enc->bank = 0;
}
