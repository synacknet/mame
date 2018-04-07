// license:BSD-3-Clause
// copyright-holders:R. Belmont

#ifndef MAME_BUS_HPDIO_98644_H
#define MAME_BUS_HPDIO_98644_H

#pragma once

#include "hp_dio.h"
#include "machine/ins8250.h"

#define RS232_TAG       "rs232"
#define INS8250_TAG     "ins8250"

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> dio16_98644_device

class dio16_98644_device :
		public device_t,
		public device_dio16_card_interface
{
public:
	// construction/destruction
	dio16_98644_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	DECLARE_READ16_MEMBER(io_r);
	DECLARE_WRITE16_MEMBER(io_w);

	required_device<ins8250_device> m_uart;

protected:
	dio16_98644_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);


	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	virtual ioport_constructor device_input_ports() const override;
	// optional information overrides
	virtual void device_add_mconfig(machine_config &config) override;

private:
	required_ioport m_switches;
	uint8_t m_control;

	bool m_loopback;
	uint8_t  m_data;
};

// device type definition
DECLARE_DEVICE_TYPE(HPDIO_98644, dio16_98644_device)

#endif // MAME_BUS_HPDIO_98644_H