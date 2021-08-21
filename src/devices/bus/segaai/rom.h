// license:BSD-3-Clause
// copyright-holders:Wilbert Pol
#ifndef MAME_BUS_SEGAAI_ROM_H
#define MAME_BUS_SEGAAI_ROM_H

#pragma once

#include "segaai_slot.h"

class segaai_rom_128_device : public device_t,
						public device_segaai_card_interface
{
public:
	segaai_rom_128_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	virtual void device_start();
	virtual void device_reset();

	virtual u8 read_cart(offs_t offset);
	virtual void write_cart(offs_t offset, u8 data);

protected:
	segaai_rom_128_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock);
};


class segaai_rom_256_device : public segaai_rom_128_device
{
public:
	segaai_rom_256_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	virtual void device_start();
	virtual void device_reset();

	virtual u8 read_cart(offs_t offset);
	virtual void write_cart(offs_t offset, u8 data);

protected:
	u8 m_bank_reg[4];
};


DECLARE_DEVICE_TYPE(SEGAAI_ROM_128, segaai_rom_128_device);
DECLARE_DEVICE_TYPE(SEGAAI_ROM_256, segaai_rom_256_device);

#endif
