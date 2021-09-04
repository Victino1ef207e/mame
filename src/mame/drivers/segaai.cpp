// license:BSD-3-Clause
// copyright-holders:Wilbert Pol, Fabio Priuli
// thanks-to:Chris Covell
/*
 
 Sega AI driver

 
 Not much is known at this stage, except that the system was intended to be
 used for educational purposes in schools. Yet the audio chips seem much more
 powerful than what an educational computer requires...

 CPU : 16bit V20 @ 5MHz
 ROM : 128KB OS.with SEGA PROLOG
 RAM : 128KB
 VRAM : 64KB
 Video : V9938 Resolution 256x212
 Sound : SN76489
 Cassette Drive : 9600bps
 TV Output : RGB, Video, RF
 Keyboard : new JIS arrangement (Japanese input mapping)

 
TODO:
- The artwork system has no support for a real touchpad device with
  selectable artwork, so the touchpad is emulated as a 24x20 matrix
  of clickable buttons. This is currently good enough to make most
  games playable. Eventually this should behave like a real touchpad
  so also drawing apps can work.
- IRQ enable/disable register
- Proper hooking up of uPD7759 and DRQ signals in slave mode.
- Proper hooking up of uPD7759 START signal.
- Cassette
- Keyboard (there is probably an mcu inside it)
- State saving

===========================================================================

 Sega AI Computer quick PCB overview by Chris Covell
 
 Major ICs 
 
 IC 1    D701080-5     (86/09?)  NEC V20 CPU       DIP40 
 IC 2    315-5200      (86/25)   SEGA          QFP100 
 IC 3    27C512-25     (86/15)   64K EPROM "E000  8/24" 
 IC 4    27C512-25     (86/06)   64K EPROM "F000  7/21" 
 IC 5    MPR-7689      (86/22)   SEGA "264 AA E79" (ROM) DIP28
 IC 10   V9938                   Yamaha MSX2 VDP 
 IC 13   D7759C        (86/12)   NEC Speech Synthesizer   DIP40 
 IC 14   MPR-7619      (86/23)   SEGA (ROM)      DIP28
 IC 15   MPR-7620      (86/23)   SEGA (ROM)      DIP28
 IC 16   SN76489AN               TI PSG         DIP16 
 IC 17   D8251AFC      (86/09)   NEC Communications Interface DIP28 
 IC 18   315-5201      (86/25)   SEGA (bodge wire on pins 9,10) DIP20 
 IC 19   M5204A        (87?/01)  OKI 
 IC 20   D8255AC-2     (86/08)   NEC Peripheral Interface DIP40 
 
 IC 6,7,8,9,11,12   D41464C-12   NEC 32K DRAMs - 128K RAM, 64K VRAM 
 
 Crystals, etc 
 
 X1   20.000           "KDS 6D" 
 X2   21.47727         "KDS" 
 X3   640kHz           "CSB 640 P" 
 
 Connectors 
 
 CN1   6-pin DIN Power connector 
 CN2   8-pin DIN "AUX" connector 
 CN3   Video phono jack 
 CN4   Audio phono jack 
 CN5   35-pin Sega MyCard connector 
 CN6   60-pin expansion connector A1..A30 Bottom, B1..B30 Top 
 CN7   9-pin header connector to "Joy, Button, LED" unit 
 CN8   13(?) pin flat flex connector to pressure pad 
 CN9   9-pin header connector to tape drive motor, etc. 
 CN10   13-pin header connector to tape heads 
 JP2   2-wire header to SW2 button board 
 PJ1   7-wire header to Keyboard / Mic connector board 
 MIC   2-wire header to mic on KB/Mic board 
 SW1   Reset Switch 
 
 Power switch is on the AC Adaptor 
 
 Joypad unit (by Mitsumi) has U/D/L/R, "PL" and "PR" buttons, and a power LED. 
 
Power Connector Pinout (Seen from AC Adaptor plug):                
   1     5        1  12V COM    5   5V COM 
      6           2  12V OUT    6   5V OUT 
   2     4        3   5V COM        
      3           4   5V OUT 

AUX Connector Pinout: 
   7   6          1 +5V(?)      5 csync 
  3  8  1         2 GND         6 green 
   5   4          3 blue        7 Audio out 
     2            4 +5V(?)      8 red 

New JIS Keyboard Connector Pinout: 
    1 2           1,2,3 data lines 
  3 4   5         4 ??          5,8 data lines 
   6 7 8          6 GND         7 +5V
 

*/ 

#include "emu.h"
#include "cpu/nec/nec.h"
#include "cpu/z80/z80.h"
#include "sound/sn76496.h"
#include "sound/upd7759.h"
#include "video/v9938.h"
#include "bus/segaai/segaai_slot.h"
#include "bus/segaai/segaai_exp.h"
#include "machine/i8255.h"
#include "machine/i8251.h"
#include "speaker.h"
#include "softlist.h"

// Layout
#include "segaai.lh"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"


#define TOUCHPAD_ROWS 20


class segaai_state : public driver_device
{
public:
	segaai_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_sound(*this, "sn76489a")
		, m_v9938(*this, "v9938")
		, m_upd7759(*this, "upd7759")
		, m_port4(*this, "PORT4")
		, m_port5(*this, "PORT5")
		, m_port_tp(*this, "TP.%u", 0)
		, m_vector(0)
	{ }
	
	void segaai(machine_config &config);

	DECLARE_WRITE_LINE_MEMBER(vdp_interrupt);
	DECLARE_WRITE_LINE_MEMBER(upd7759_drq_w);
	DECLARE_WRITE_LINE_MEMBER(upd7759_busy_w);
	IRQ_CALLBACK_MEMBER(irq_callback);
	u8 i8255_porta_r();
	u8 i8255_portb_r();
	u8 i8255_portc_r();
	void i8255_portc_w(u8 data);
	void upd7759_ctrl_w(offs_t offset, u8 data);
	void port1c_w(offs_t offset, u8 data);
	void port1d_w(offs_t offset, u8 data);
	void port1e_w(offs_t offset, u8 data);
	u8 port1e_r(offs_t offset);

	// unknown device writes
	u8 irq_enable_r(offs_t offset);
	void irq_enable_w(offs_t offset, u8 data);
	void irq_select_w(offs_t offset, u8 data);

protected:
	virtual void machine_start();

private:
	static constexpr u8 VECTOR_V9938 = 0xf8;
	static constexpr u8 VECTOR_I8251_SEND = 0xf9;
	static constexpr u8 VECTOR_I8251_RECEIVE = 0xfa;
	static constexpr u8 VECTOR_UPD7759 = 0xfb;
	static constexpr u8 IRQ_V9938 = 0x01;
	static constexpr u8 IRQ_UPD7759 = 0x08;

	void mem_map(address_map &map);
	void io_map(address_map &map);
	void update_irq_state();
	bool get_touchpad_pressed();
	u32 get_vector() { return m_vector; }

	required_device<cpu_device> m_maincpu;
	required_device<sn76489a_device> m_sound;
	required_device<v9938_device> m_v9938;
	required_device<upd7759_device> m_upd7759;
	required_ioport m_port4;
	required_ioport m_port5;
	required_ioport_array<TOUCHPAD_ROWS> m_port_tp;

	u8 m_i8255_portb;
	u8 m_upd7759_ctrl;
	u8 m_port_1c;
	u8 m_port_1d;
	u8 m_port_1e;
	int m_prev_v9938_irq;
	int m_prev_upd7759_irq;
	u8 m_touchpad_x;
	u8 m_touchpad_y;
	u8 m_irq_active;
	u8 m_irq_enabled;
	u32 m_vector;
};


void segaai_state::mem_map(address_map &map)
{
	map(0x00000, 0x1ffff).ram();
	map(0x20000, 0x3ffff).rw("exp", FUNC(segaai_exp_slot_device::read_lo), FUNC(segaai_exp_slot_device::write_lo));
	map(0x80000, 0x8ffff).rw("exp", FUNC(segaai_exp_slot_device::read_hi), FUNC(segaai_exp_slot_device::write_hi));
	map(0xa0000, 0xbffff).rw("cardslot", FUNC(segaai_card_slot_device::read_cart), FUNC(segaai_card_slot_device::write_cart));
	map(0xc0000, 0xdffff).rom();
	map(0xe0000, 0xeffff).rom();
	map(0xf0000, 0xfffff).rom();
}


/*
Interesting combination of I/O actions from the BIOS:

EC267: B0 03                mov     al,3h
EC269: E6 17                out     17h,al
EC26B: B0 FC                mov     al,0FCh		; 11111100
EC26D: E6 0F                out     0Fh,al
EC26F: B0 FF                mov     al,0FFh
EC271: E6 08                out     8h,al

same code at ECDBE, ED2FC
EC2D6: B0 05                mov     al,5h
EC2D8: E6 17                out     17h,al
EC2DA: B0 FA                mov     al,0FAh		; 11111010
EC2DC: E6 0F                out     0Fh,al
EC2DE: B0 00                mov     al,0h
EC2E0: E4 08                in      al,8h

same code at ECE08, ECE1D, ED282, EDBA8, EDD78
EC319: B0 04                mov     al,4h
EC31B: E6 17                out     17h,al
EC31D: B0 FE                mov     al,0FEh		; 11111110
EC31F: E6 0F                out     0Fh,al

ECB45: 80 FA 03             cmp     dl,3h
ECB48: 74 05                be      0ECB4Fh
ECB4A: B0 09                mov     al,9h
ECB4C: E9 02 00             br      0ECB51h
ECB4F: B0 08                mov     al,8h
ECB51: E6 17                out     17h,al

same code at ED02A, ED17E, ED1DC
ECEE5: B0 03                mov     al,3h
ECEE7: E6 17                out     17h,al
ECEE9: B0 FC                mov     al,0FCh		; 11111100
ECEEB: E6 0F                out     0Fh,al
ECEED: B0 00                mov     al,0h
ECEEF: E6 08                out     8h,al

same code at ED0D9, ED120, EDB04, EDC8F
ECF0D: B0 02                mov     al,2h
ECF0F: E6 17                out     17h,al
ECF11: B0 FE                mov     al,0FEh		; 11111110
ECF13: E6 0F                out     0Fh,al

ECF35: B0 08                mov     al,8h
ECF37: E6 17                out     17h,al

ED673: B0 07                mov     al,7h
ED675: E6 17                out     17h,al
ED677: B0 01                mov     al,1h
+ out     0Bh,al?

ED683: B0 06                mov     al,6h
ED685: E6 17                out     17h,al
ED687: B0 00                mov     al,0h
+ out     0Bh,al?

EDBC4: B0 0A                mov     al,0Ah
EDBC6: E6 17                out     17h,al

EDBD1: 24 01                and     al,1h
EDBD3: 04 0A                add     al,0Ah
EDBD5: E6 17                out     17h,al

EE01E: B0 08                mov     al,8h           ; brk #31, iy == 01
EE020: 83 FF 01             cmp     iy,1h
EE023: 74 02                be      0EE027h
EE025: B0 09                mov     al,9h           ; brk #31, iy == 00
EE027: E6 17                out     17h,al

*/

void segaai_state::io_map(address_map &map)
{
	map(0x00, 0x03).rw(m_v9938, FUNC(v9938_device::read), FUNC(v9938_device::write));
	map(0x04, 0x07).rw("tmp8255", FUNC(i8255_device::read), FUNC(i8255_device::write));

	map(0x08, 0x08).rw("i8251", FUNC(i8251_device::data_r), FUNC(i8251_device::data_w));
	map(0x09, 0x09).rw("i8251", FUNC(i8251_device::status_r), FUNC(i8251_device::control_w));

	// 0x0a (w) - ??
	// 0a: 00 written during boot
	map(0x0b, 0x0b).w(FUNC(segaai_state::upd7759_ctrl_w));    // 315-5201

	map(0x0c, 0x0c).w(m_sound, FUNC(sn76489a_device::write));

	// 0x0e (w) - ??
	// 0x0f (w) - ??
	// during boot:
	// 0e <- 13
	// 0f <- ff
	// 0f <- 07
	// 0e <- 07
	// 0e <- 08
	// 0f <- fe

	map(0x14, 0x14).mirror(0x01).w(m_upd7759, FUNC(upd7759_device::port_w));

	// IRQ Enable
	map(0x16, 0x16).rw(FUNC(segaai_state::irq_enable_r), FUNC(segaai_state::irq_enable_w));
	// IRQ Enable (per IRQ source selection) Why 2 registers for IRQ enable?
	map(0x17, 0x17).w(FUNC(segaai_state::irq_select_w));

	// Touchpad
	map(0x1c, 0x1c).w(FUNC(segaai_state::port1c_w));
	map(0x1d, 0x1d).w(FUNC(segaai_state::port1d_w));
	map(0x1e, 0x1e).rw(FUNC(segaai_state::port1e_r), FUNC(segaai_state::port1e_w));

	// 0x1f (w) - ??

	// Expansion I/O
	map(0x20, 0x3f).rw("exp", FUNC(segaai_exp_slot_device::read_io), FUNC(segaai_exp_slot_device::write_io));
}


#define INPUT_TP_ROW(row) \
	PORT_START(row) \
	PORT_BIT(0x000001, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000002, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000004, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000008, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000010, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000020, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000040, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000080, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000100, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000200, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000400, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x000800, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x001000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x002000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x004000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x008000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x010000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x020000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x040000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x080000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x100000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x200000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x400000, IP_ACTIVE_HIGH, IPT_OTHER) \
	PORT_BIT(0x800000, IP_ACTIVE_HIGH, IPT_OTHER)

static INPUT_PORTS_START(ai_kbd)
	PORT_START("PORT4")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP) PORT_8WAY
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN) PORT_8WAY
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT) PORT_8WAY
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT) PORT_8WAY
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_BUTTON2) PORT_NAME("PL")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_BUTTON1) PORT_NAME("RL")
	PORT_BIT(0xc0, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("PORT5")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_BUTTON3) PORT_NAME("Grey Button")
	PORT_BIT(0xfe, IP_ACTIVE_LOW, IPT_UNUSED)

	// Touchpad
	INPUT_TP_ROW("TP.0")
	INPUT_TP_ROW("TP.1")
	INPUT_TP_ROW("TP.2")
	INPUT_TP_ROW("TP.3")
	INPUT_TP_ROW("TP.4")
	INPUT_TP_ROW("TP.5")
	INPUT_TP_ROW("TP.6")
	INPUT_TP_ROW("TP.7")
	INPUT_TP_ROW("TP.8")
	INPUT_TP_ROW("TP.9")
	INPUT_TP_ROW("TP.10")
	INPUT_TP_ROW("TP.11")
	INPUT_TP_ROW("TP.12")
	INPUT_TP_ROW("TP.13")
	INPUT_TP_ROW("TP.14")
	INPUT_TP_ROW("TP.15")
	INPUT_TP_ROW("TP.16")
	INPUT_TP_ROW("TP.17")
	INPUT_TP_ROW("TP.18")
	INPUT_TP_ROW("TP.19")
INPUT_PORTS_END


// Based on edge triggers, level triggers are created
void segaai_state::update_irq_state()
{
	int state = CLEAR_LINE;

	if (m_irq_active & m_irq_enabled)
	{
		state = ASSERT_LINE;
	}

	m_maincpu->set_input_line(0, state);
}


WRITE_LINE_MEMBER(segaai_state::vdp_interrupt)
{
	if (state != CLEAR_LINE)
	{
		if (m_prev_v9938_irq == CLEAR_LINE)
		{
			m_irq_active |= IRQ_V9938;
		}
	}
	m_prev_v9938_irq = state;

	update_irq_state();
}


WRITE_LINE_MEMBER(segaai_state::upd7759_drq_w)
{
	int upd7759_irq = state ? CLEAR_LINE : ASSERT_LINE;
	if (upd7759_irq != CLEAR_LINE)
	{
		if (m_prev_upd7759_irq == CLEAR_LINE)
		{
			m_irq_active |= IRQ_UPD7759;
		}
	}
	m_prev_upd7759_irq = upd7759_irq;

	update_irq_state();
}


WRITE_LINE_MEMBER(segaai_state::upd7759_busy_w)
{
	if (!(m_upd7759_ctrl & 0x01))
	{
//		m_0xfb_irq = state ? CLEAR_LINE : ASSERT_LINE;
//		update_irq_state();
	}
}


IRQ_CALLBACK_MEMBER(segaai_state::irq_callback)
{
	int vector = 0;	// default??

	if (m_irq_active & m_irq_enabled & IRQ_V9938)
	{
		vector = VECTOR_V9938;
		m_irq_active &= ~IRQ_V9938;
	}
	else if (m_irq_active & m_irq_enabled & IRQ_UPD7759)
	{
		vector = VECTOR_UPD7759;
		m_irq_active &= ~IRQ_UPD7759;
	}
	else
	{
		if (m_irq_active & m_irq_enabled)
		{
			fatalerror("Unknown irq triggered: $%02X active, $%02X enabled\n", m_irq_active, m_irq_enabled);
		}
		fatalerror("irq_callback called but no irq active or enabled: $%02X active, $%02X enabled\n", m_irq_active, m_irq_enabled);
	}

	m_vector = vector;

	update_irq_state();
	return vector;
}


/*
Mainboard 8255 port A

 76543210
 +-------- Microphone sensor (1 = sound enabled)
  +------- Unknown (usually 1) // -BUSY output from the uPD7759?
   +------ PR trigger (active low)
    +----- PL trigger (active low)
     +---- Pad right (active low)
      +--- Pad lefta (active low)
       +-- Pad down (active low)
        +- Pad up (active low)
*/
u8 segaai_state::i8255_porta_r()
{
	u8 data = (m_upd7759->busy_r() ? 0x40 : 0) | (m_port4->read() & ~0x40);

	return data;
}


/*
Mainboard 8255 port B

 76543210
 +-------- CN9 Pin 8 (1 - unit is powered??)
  +------- Tape head engaged
   +------ Tape insertion sensor (0 - tape is inserted, 1 - no tape inserted)
    +----- Tape write enable sensor
     +---- keyboard connector pin 3
      +--- 0 = Touch pad data available
       +-- 0 = Touch pad pressed
        +- Trigger button near touch panel (active low)
*/
u8 segaai_state::i8255_portb_r()
{
	m_i8255_portb = (m_i8255_portb & 0xf8) | (m_port5->read() & 0x01);

	if (m_port_1d & 0x01)
	{
		if (!get_touchpad_pressed())
		{
			m_i8255_portb |= 0x02;
		}

		m_i8255_portb |= 0x04;
	}
	else
	{
		m_i8255_portb |= 0x02;
		// Bit 2 reset to indicate that touchpad data is available
	}

	// when checking whether the tape is running Popoland wants to see bit7 set and bit5 reset
	// toggling this stops eigogam2 from booting normally into a game.
//	m_i8255_portb ^= 0x80;

	return (m_i8255_portb & 0xdf)/* | 0x80*/;
}


bool segaai_state::get_touchpad_pressed()
{
	static const u8 tp_x[24] =
	{
		  5,  15,  26,  37,  47,  58,  69,  79,  90, 101, 111, 122,
		133, 143, 154, 165, 175, 186, 197, 207, 218, 229, 239, 250
	};

	static const u8 tp_y[20] =
	{
		  6,  18,  31,  44,  57,  70,  82,  95, 108, 121,
		134, 146, 159, 172, 185, 198, 210, 223, 236, 249
	};

	for (int row = 0; row < TOUCHPAD_ROWS; row++)
	{
		u32 port = m_port_tp[row]->read();

		if (port)
		{
			int bit = -1;
			while (port)
			{
				bit++;
				port >>= 1;
			}

			if (bit >= 0 && bit < std::size(tp_x))
			{
				m_touchpad_x = tp_x[bit];
				m_touchpad_y = tp_y[row];
				return true;
			}
		}
	}

	return false;
}


/*
Mainboard 8255 port C

 76543210
 +-------- keyboard connector pin 5
  +------- keyboard connector pin 8
   +------ keyboard connector pin 2
    +----- keyboard connector pin 1
     +---- Output
      +--- Output
       +-- Output
        +- Output
*/
u8 segaai_state::i8255_portc_r()
{
	u8 data = 0xf0;

	return data;
}


void segaai_state::i8255_portc_w(u8 data)
{
	LOG("i8255 port c write: %02x\n", data);
}


void segaai_state::upd7759_ctrl_w(offs_t offset, u8 data)
{
	LOG("I/O Port $0b write: $%02x\n", data);

	m_upd7759_ctrl = data;

	// bit0 is connected to /md line of the uPD7759
	m_upd7759->md_w((m_upd7759_ctrl & 0x01) ? 0 : 1);

	// bit1 selects which ROM should be used?
//	m_upd7759->set_bank_base((m_upd7759_ctrl & 2) ? 0x00000 : 0x20000);
	// TODO check if this is correct
	m_upd7759->set_rom_bank((m_upd7759_ctrl & 2) >> 1);
}


// I/O Port 16 - IRQ Enable
u8 segaai_state::irq_enable_r(offs_t offset)
{
	return m_irq_enabled;
}


// IRQ Enable
// 76543210
// +-------- ???
//  +------- ???
//   +------ ???
//    +----- ???
//     +---- D7759 IRQ enable
//      +--- ???
//       +-- ???
//        +- V9938 IRQ enable
void segaai_state::irq_enable_w(offs_t offet, u8 data)
{
	m_irq_enabled = data;
}

// I/O Port 17 - IRQ Enable selection
/*

Port 16 and 17 are closely related (IRQ Enable/State?)

Some config can be written through port 17, and the current combined
settings can be read through port 16. From the bios code no such relation
is directly clear though.

See these snippets from eigogam2:
A9EC5: FA                        di
A9EC6: E4 16                     in      al,16h
A9EC8: A2 82 12                  mov     [1282h],al
A9ECB: B0 00                     mov     al,0h
A9ECD: E6 17                     out     17h,al
A9ECF: B0 02                     mov     al,2h
A9ED1: E6 17                     out     17h,al
A9ED3: B0 04                     mov     al,4h
A9ED5: E6 17                     out     17h,al
A9ED7: B0 07                     mov     al,7h
A9ED9: E6 17                     out     17h,al
A9EDB: B0 0D                     mov     al,0Dh
A9EDD: E6 17                     out     17h,al
A9EDF: B0 0E                     mov     al,0Eh
A9EE1: E6 17                     out     17h,al
A9EE3: FB                        ei
...
A9F05: B0 06                     mov     al,6h
A9F07: E6 17                     out     17h,al
A9F09: B0 0D                     mov     al,0Dh
A9F0B: E6 17                     out     17h,al
A9F0D: A0 82 12                  mov     al,[1282h]
A9F10: D0 C0                     rol     al,1
A9F12: 24 01                     and     al,1h
A9F14: 04 0E                     add     al,0Eh
A9F16: E6 17                     out     17h,al
A9F18: A0 82 12                  mov     al,[1282h]
A9F1B: D0 C0                     rol     al,1
A9F1D: D0 C0                     rol     al,1
A9F1F: 24 01                     and     al,1h
A9F21: 04 0C                     add     al,0Ch
A9F23: E6 17                     out     17h,al
A9F25: 8A 26 82 12               mov     ah,[1282h]
A9F29: 32 DB                     xor     bl,bl
A9F2B: B9 03 00                  mov     cw,3h
A9F2E: 8A C4                     mov     al,ah
A9F30: 24 01                     and     al,1h
A9F32: 02 C3                     add     al,bl
A9F34: E6 17                     out     17h,al
A9F36: D0 EC                     shr     ah,1
A9F38: 80 C3 02                  add     bl,2h
A9F3B: E2 F1                     dbnz    0A9F2Eh
*/
// In gulliver 0000 1010 is written shortly after writing a byte to I/O port 14
void segaai_state::irq_select_w(offs_t offset, u8 data)
{
	int pin = (data >> 1) & 0x07;
	if (data & 1)
	{
		m_irq_enabled |= (1 << pin);
	}
	else
	{
		m_irq_enabled &= ~(1 << pin);
	}
}


void segaai_state::port1c_w(offs_t offset, u8 data)
{
	m_port_1c = data;
}


void segaai_state::port1d_w(offs_t offset, u8 data)
{
	m_port_1d = data;
}


void segaai_state::port1e_w(offs_t offset, u8 data)
{
	m_port_1e = data;
}


u8 segaai_state::port1e_r(offs_t offset)
{
	if (m_port_1c & 0x01)
	{
		return m_touchpad_y;
	}
	else
	{
		return m_touchpad_x;
	}
}


void segaai_state::machine_start()
{
	m_i8255_portb = 0x7f;
	m_upd7759_ctrl = 0;
	m_port_1c = 0;
	m_port_1d = 0;
	m_port_1e = 0;
	m_prev_v9938_irq = CLEAR_LINE;
	m_prev_upd7759_irq = CLEAR_LINE;
	m_touchpad_x = 0;
	m_touchpad_y = 0;
	m_vector = 0;
	m_irq_enabled = 0;
	m_irq_active = 0;
}


void segaai_state::segaai(machine_config &config)
{
	V20(config, m_maincpu, 20_MHz_XTAL/4);
	m_maincpu->set_addrmap(AS_PROGRAM, &segaai_state::mem_map);
	m_maincpu->set_addrmap(AS_IO, &segaai_state::io_map);
	// TODO enough, or do we also need to add a vector callback?
	m_maincpu->set_irq_acknowledge_callback(FUNC(segaai_state::irq_callback));
	// TODO
//	m_maincpu->vector_cb().set(FUNC(segaai_state::get_vector));

	V9938(config, m_v9938, 21.477272_MHz_XTAL);
	m_v9938->set_screen_ntsc("screen");
	m_v9938->set_vram_size(0x10000);
	m_v9938->int_cb().set(FUNC(segaai_state::vdp_interrupt));
	SCREEN(config, "screen", SCREEN_TYPE_RASTER);

	i8255_device &tmp8255(I8255(config, "tmp8255"));
	tmp8255.in_pa_callback().set(FUNC(segaai_state::i8255_porta_r));
	tmp8255.in_pb_callback().set(FUNC(segaai_state::i8255_portb_r));
	tmp8255.in_pc_callback().set(FUNC(segaai_state::i8255_portc_r));
	tmp8255.out_pc_callback().set(FUNC(segaai_state::i8255_portc_w));

	I8251(config, "i8251", 0);

	SPEAKER(config, "mono").front_center();

	SN76489A(config, m_sound, 21.477272_MHz_XTAL/6); // not verified, but sounds close to real hw recordings
	m_sound->add_route(ALL_OUTPUTS, "mono", 1.00);

	UPD7759(config, m_upd7759);
	m_upd7759->add_route(ALL_OUTPUTS, "mono", 1.00);
	m_upd7759->drq().set(FUNC(segaai_state::upd7759_drq_w));
	// TODO after upd7759 updates
//	m_upd7759->busy().set(FUNC(segaai_state::upd7759_busy_w));

	// Card slot
	SEGAAI_CARD_SLOT(config, "cardslot", segaai_card, nullptr);
	SOFTWARE_LIST(config, "software").set_original("segaai");

	// Expansion slot
	SEGAAI_EXP_SLOT(config, "exp", segaai_exp, "soundbox");

	config.set_default_layout(layout_segaai);
}


ROM_START(segaai)
	ROM_REGION(0x100000, "maincpu", 0)
	ROM_LOAD("mpr-7689.ic5",  0xc0000, 0x20000, CRC(62402ac9) SHA1(bf52d22b119d54410dad4949b0687bb0edf3e143))
	ROM_LOAD("e000 8_24.ic3", 0xe0000, 0x10000, CRC(c8b6a539) SHA1(cbf8473d1e3d8037ea98e9ca8b9aafdc8d16ff23))	// actual label was "e000 8/24"
	ROM_LOAD("f000 7_21.ic4", 0xf0000, 0x10000, CRC(64d6cd8c) SHA1(68c130048f16d6a0abe1978e84440931470222d9))	// actual label was "f000 7/21"

	ROM_REGION(0x40000, "upd7759", 0)
	ROM_LOAD("mpr-7619.ic14", 0x00000, 0x20000, CRC(d1aea002) SHA1(c8d5408bba65b17301f19cf9ebd2b635d642525a))
	ROM_LOAD("mpr-7620.ic15", 0x20000, 0x20000, CRC(e042754b) SHA1(02aede7a3e2fda9cbca621b530afa4520cf16610))
ROM_END


COMP(1986, segaai,     0,         0,      segaai,   ai_kbd, segaai_state,   empty_init,    "Sega",   "AI", MACHINE_NOT_WORKING)
