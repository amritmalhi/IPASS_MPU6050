/**
 * \file      hwlib-glcd-oled-spi.hpp
 * \brief     SPI OLED driver for 128x64 screens.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2016, Chris Smeele
 * \date      2016-10-06
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at 
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * This file incorporates work covered by the following copyright and
 * license notice:
 */
// ==========================================================================
//
// Copyright : wouter@voti.nl 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// ==========================================================================
/// @file
#pragma once

/* Usage:
 *
 *     auto mosi  = target::pin_out(target::pins::d9);
 *     auto sclk  = target::pin_out(target::pins::d10);
 *     auto dc    = target::pin_out(target::pins::d11);
 *     auto cs    = target::pin_out(target::pins::d12);
 *     auto reset = target::pin_out(target::pins::d13);
 *     auto miso  = target::pin_in (target::pins::d5);
 *
 *     auto spi = hwlib::spi_bus_bit_banged_sclk_mosi_miso(sclk, mosi, miso);
 *
 *     hwlib::glcd_oled_spi oled(spi, cs, dc, reset);
 *     oled.write({6, 6}, oled.foreground);
 *     oled.flush();
 *
 * In this example the pins on the OLED are connected as follows:
 * 
 * | OLED | DUE |
 * |------+-----|
 * | Data | d9  |
 * | Clk  | d10 |
 * | DC   | d11 |
 * | Rst  | d13 |
 * | CS   | d12 |
 * | Vin  | 5V  |
 * | Gnd  | GND |
 *
 * Note, you can also pass a 128*64/8 -byte buffer to the flush function to flush that to the screen instead.
 */

#include "hwlib.hpp"
#include <hwlib-graphics.hpp>

namespace hwlib {

class glcd_oled_spi : public window {
private:
    spi_bus &bus;
    pin_out &dc;
    pin_out &cs;
    pin_out &reset;

    enum command_t : uint8_t {
        SETCONTRAST                          = 0x81,
        DISPLAYALLON_RESUME                  = 0xA4,
        DISPLAYALLON                         = 0xA5,
        NORMALDISPLAY                        = 0xA6,
        INVERTDISPLAY                        = 0xA7,
        DISPLAYOFF                           = 0xAE,
        DISPLAYON                            = 0xAF,
        SETDISPLAYOFFSET                     = 0xD3,
        SETCOMPINS                           = 0xDA,
        SETVCOMDETECT                        = 0xDB,
        SETDISPLAYCLOCKDIV                   = 0xD5,
        SETPRECHARGE                         = 0xD9,
        SETMULTIPLEX                         = 0xA8,
        SETLOWCOLUMN                         = 0x00,
        SETHIGHCOLUMN                        = 0x10,
        SETSTARTLINE                         = 0x40,
        MEMORYMODE                           = 0x20,
        COLUMNADDR                           = 0x21,
        PAGEADDR                             = 0x22,
        COMSCANINC                           = 0xC0,
        COMSCANDEC                           = 0xC8,
        SEGREMAP                             = 0xA0,
        CHARGEPUMP                           = 0x8D,
        EXTERNALVCC                          = 0x01,
        SWITCHCAPVCC                         = 0x02,
        ACTIVATE_SCROLL                      = 0x2F,
        DEACTIVATE_SCROLL                    = 0x2E,
        SET_VERTICAL_SCROLL_AREA             = 0xA3,
        RIGHT_HORIZONTAL_SCROLL              = 0x26,
        LEFT_HORIZONTAL_SCROLL               = 0x27,
        VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL = 0x29,
        VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  = 0x2A,   
    };

    uint8_t buffer[128 * 64 / 8] = { };
	//Added buffering mode, seeing as this might be new this year...
    void write_implementation(location pos, color col, buffering buf = buffering::unbuffered) override {
        const int i = pos.x + (pos.y / 8) * size.x;

        if (col == foreground){ 
            buffer[i] |=  (0x01 << (pos.y % 8));  
        } else {
            buffer[i] &= ~(0x01 << (pos.y % 8)); 
        }   
    }

    void write_cmd(uint8_t cmd) {
        uint8_t out;
        bus.write_and_read(cs, 1, &cmd, &out);
    }

	//Changed this around to be hardcoded, seeing as initializer list didn't work for me
    void write_cmd(uint8_t cmd, uint8_t arg0, uint8_t arg1) {
        write_cmd(cmd);
        write_cmd(arg0);
		write_cmd(arg1);
    }

    void write_cmds(const uint8_t *buf, uint32_t count) {
        constexpr uint32_t at_a_time = 64;

        uint8_t outbuf[at_a_time];
        for (uint32_t i = 0; i < count; ) {

            uint32_t to_write = count - i < at_a_time
                    ? count - i
                    : at_a_time;

            bus.write_and_read(cs, to_write, buf, outbuf);

            i += to_write;
        }
    }

public:

    void flush(uint8_t *buffer = nullptr) {
        if (!buffer)
            buffer = this->buffer;

        write_cmd(COLUMNADDR,  0,  size.x-1);
        write_cmd(PAGEADDR,    0,  7);   

        // Brace yourselves, data is coming.
        dc.set(true);

        // Write 64 bytes at a time.
        uint8_t outbuf[64];

        for (unsigned i = 0; i < sizeof(this->buffer); i += sizeof(outbuf))
            bus.write_and_read(cs, sizeof(outbuf), buffer + i, outbuf);

        dc.set(false);
    }

    void clear(
		buffering buf = buffering::unbuffered
	) override {
        for (unsigned i = 0; i < sizeof(buffer)/sizeof(*buffer); i++)
            buffer[i] = 0;
    }
    
    glcd_oled_spi(spi_bus &bus, pin_out &cs, pin_out &dc, pin_out &reset)
        : window(location(128, 64), black, white),
          bus(bus),
          dc(dc),
          cs(cs),
          reset(reset)
    {
        cs.set(false);
        dc.set(false);

        reset.set(true);
        wait_ms(10);
        reset.set(false);
        wait_ms(10);
        reset.set(true);

        cs.set(true);
        wait_ms(1);

        static constexpr const uint8_t init_sequence[] = {
            DISPLAYOFF,
            SETDISPLAYCLOCKDIV,   0x80,
            SETMULTIPLEX,         64 - 1, // Window height, which is const.
            SETDISPLAYOFFSET,     0x00,
            SETSTARTLINE        | 0x00,
            CHARGEPUMP,           0x14,
            MEMORYMODE,           0x00,
            SEGREMAP            | 0x01,
            COMSCANDEC,
            SETCOMPINS,           0x12,
            SETCONTRAST,          0xCF,
            SETPRECHARGE,         0xF1,
            SETVCOMDETECT,        0x40,
            DISPLAYALLON_RESUME,
            NORMALDISPLAY,
            DISPLAYON,
        };

        wait_ms(20);

        write_cmds(init_sequence, sizeof(init_sequence) / sizeof(*init_sequence));
    }
};

}