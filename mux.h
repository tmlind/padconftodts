/*
 * Copyright (C) 2009 Nokia
 * Copyright (C) 2009-2010 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "mux2420.h"
#include "mux2430.h"
#include "mux34xx.h"
#include "mux44xx.h"

#define OMAP_MUX_TERMINATOR	0xffff

/* 34xx mux mode options for each pin. See TRM for options */
#define OMAP_MUX_MODE0      0
#define OMAP_MUX_MODE1      1
#define OMAP_MUX_MODE2      2
#define OMAP_MUX_MODE3      3
#define OMAP_MUX_MODE4      4
#define OMAP_MUX_MODE5      5
#define OMAP_MUX_MODE6      6
#define OMAP_MUX_MODE7      7

/* 24xx/34xx mux bit defines */
#define OMAP_PULL_ENA			(1 << 3)
#define OMAP_PULL_UP			(1 << 4)
#define OMAP_ALTELECTRICALSEL		(1 << 5)

/* omap3/4/5 specific mux bit defines */
#define OMAP_INPUT_EN			(1 << 8)
#define OMAP_OFF_EN			(1 << 9)
#define OMAP_OFFOUT_EN			(1 << 10)
#define OMAP_OFFOUT_VAL			(1 << 11)
#define OMAP_OFF_PULL_EN		(1 << 12)
#define OMAP_OFF_PULL_UP		(1 << 13)
#define OMAP_WAKEUP_EN			(1 << 14)
#define OMAP_WAKEUP_EVENT		(1 << 15)

/* Active pin states */
#define OMAP_PIN_OUTPUT			0
#define OMAP_PIN_INPUT			OMAP_INPUT_EN
#define OMAP_PIN_INPUT_PULLUP		(OMAP_PULL_ENA | OMAP_INPUT_EN \
						| OMAP_PULL_UP)
#define OMAP_PIN_INPUT_PULLDOWN		(OMAP_PULL_ENA | OMAP_INPUT_EN)

/* Off mode states */
#define OMAP_PIN_OFF_NONE		0
#define OMAP_PIN_OFF_OUTPUT_HIGH	(OMAP_OFF_EN | OMAP_OFFOUT_EN \
						| OMAP_OFFOUT_VAL)
#define OMAP_PIN_OFF_OUTPUT_LOW		(OMAP_OFF_EN | OMAP_OFFOUT_EN)
#define OMAP_PIN_OFF_INPUT_PULLUP	(OMAP_OFF_EN | OMAP_OFF_PULL_EN \
						| OMAP_OFF_PULL_UP)
#define OMAP_PIN_OFF_INPUT_PULLDOWN	(OMAP_OFF_EN | OMAP_OFF_PULL_EN)
#define OMAP_PIN_OFF_WAKEUPENABLE	OMAP_WAKEUP_EN

#define OMAP_MODE_GPIO(partition, x)	(((x) & OMAP_MUX_MODE7) == \
					  partition->gpio)
#define OMAP_MODE_UART(x)	(((x) & OMAP_MUX_MODE7) == OMAP_MUX_MODE0)

/* Flags for omapX_mux_init */
#define OMAP_PACKAGE_MASK		0xffff
#define OMAP_PACKAGE_CBS		8		/* 547-pin 0.40 0.40 */
#define OMAP_PACKAGE_CBL		7		/* 547-pin 0.40 0.40 */
#define OMAP_PACKAGE_CBP		6		/* 515-pin 0.40 0.50 */
#define OMAP_PACKAGE_CUS		5		/* 423-pin 0.65 */
#define OMAP_PACKAGE_CBB		4		/* 515-pin 0.40 0.50 */
#define OMAP_PACKAGE_CBC		3		/* 515-pin 0.50 0.65 */
#define OMAP_PACKAGE_ZAC		2		/* 24xx 447-pin POP */
#define OMAP_PACKAGE_ZAF		1		/* 2420 447-pin SIP */


#define OMAP_MUX_NR_MODES		8		/* Available modes */
#define OMAP_MUX_NR_SIDES		2		/* Bottom & top */

/*
 * omap_mux_init flags definition:
 *
 * OMAP_GPIO_MUX_MODE, bits 0-2: gpio muxing mode, same like pad control
 *      register which includes values from 0-7.
 * OMAP_MUX_REG_8BIT: Ensure that access to padconf is done in 8 bits.
 * The default value is 16 bits.
 */
#define OMAP_MUX_GPIO_IN_MODE0		OMAP_MUX_MODE0
#define OMAP_MUX_GPIO_IN_MODE1		OMAP_MUX_MODE1
#define OMAP_MUX_GPIO_IN_MODE2		OMAP_MUX_MODE2
#define OMAP_MUX_GPIO_IN_MODE3		OMAP_MUX_MODE3
#define OMAP_MUX_GPIO_IN_MODE4		OMAP_MUX_MODE4
#define OMAP_MUX_GPIO_IN_MODE5		OMAP_MUX_MODE5
#define OMAP_MUX_GPIO_IN_MODE6		OMAP_MUX_MODE6
#define OMAP_MUX_GPIO_IN_MODE7		OMAP_MUX_MODE7
#define OMAP_MUX_REG_8BIT		(1 << 3)

/**
 * struct omap_mux - data for omap mux register offset and it's value
 * @reg_offset:	mux register offset from the mux base
 * @gpio:	GPIO number
 * @muxnames:	available signal modes for a ball
 * @balls:	available balls on the package
 */
struct omap_mux {
	u16	reg_offset;
	u16	gpio;
	char	*muxnames[OMAP_MUX_NR_MODES];
	char	*balls[OMAP_MUX_NR_SIDES];
};

/**
 * struct omap_ball - data for balls on omap package
 * @reg_offset:	mux register offset from the mux base
 * @balls:	available balls on the package
 */
struct omap_ball {
	u16	reg_offset;
	char	*balls[OMAP_MUX_NR_SIDES];
};

/**
 * struct omap_board_mux - data for initializing mux registers
 * @reg_offset:	mux register offset from the mux base
 * @mux_value:	desired mux value to set
 */
struct omap_board_mux {
	u16	reg_offset;
	u16	value;
};

/**
 * omap2420_mux_init() - initialize mux system with board specific set
 * @board_mux:		Board specific mux table
 * @flags:		OMAP package type used for the board
 */
int omap2420_mux_init(struct omap_board_mux *board_mux, int flags);

/**
 * omap2430_mux_init() - initialize mux system with board specific set
 * @board_mux:		Board specific mux table
 * @flags:		OMAP package type used for the board
 */
int omap2430_mux_init(struct omap_board_mux *board_mux, int flags);

/**
 * omap3_mux_init() - initialize mux system with board specific set
 * @board_mux:		Board specific mux table
 * @flags:		OMAP package type used for the board
 */
int omap3_mux_init(struct omap_board_mux *board_mux, int flags);

/**
 * omap4_mux_init() - initialize mux system with board specific set
 * @board_subset:	Board specific mux table
 * @board_wkup_subset:	Board specific mux table for wakeup instance
 * @flags:		OMAP package type used for the board
 */
int omap4_mux_init(struct omap_board_mux *board_subset,
	struct omap_board_mux *board_wkup_subset, int flags);

/**
 * omap_mux_init - private mux init function, do not call
 */
int omap_mux_init(const char *name, u32 flags,
		  u32 mux_pbase, u32 mux_size,
		  struct omap_mux *superset,
		  struct omap_mux *package_subset,
		  struct omap_board_mux *board_mux,
		  struct omap_ball *package_balls);
