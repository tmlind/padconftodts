#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mux.h"

#define MAX_PADCONF_NR		2

enum soc_id {
	SOC_UNKNOWN,
	OMAP4430_ES1,
	OMAP4430_ES2,
	OMAP4460_ES1_1,
};

struct padconf {
	const char *name;
	uint32_t flags;
	uint32_t mux_pbase;
	int mux_offset;
	uint32_t mux_size;
	struct omap_mux *signals;
};

struct soc_info {
	char machine[128];
	char type[64];
	char revision[64];
	enum soc_id id;
	uint16_t package;
	struct padconf padconf[MAX_PADCONF_NR];
};

static struct soc_info soc;

static int parse_sysfs_entry(char *filename, char* buf, int size)
{
	FILE *f;
	int error = 0;

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "ERROR: Could not read %s: %i\n", filename, -errno);

		return -errno;
	}

	if (!fgets(buf, size, f)) {
		error = -ENODEV;
		goto out;
	}

	strtok(buf, "\n");

	fclose(f);

out:
	return error;
}

static int detect_system(struct soc_info *soc)
{
	int error;

	error = parse_sysfs_entry("/sys/devices/soc0/machine", soc->machine,
				  sizeof(soc->machine));
	if (error < 0)
		return error;

	error = parse_sysfs_entry("/sys/devices/soc0/revision", soc->revision,
				  sizeof(soc->revision));
	if (error < 0)
		return error;

	error = parse_sysfs_entry("/sys/devices/soc0/type", soc->type,
				  sizeof(soc->type));
	if (error < 0)
		return error;

	fprintf(stderr, "INFO: Detected %s %s %s\n", soc->machine, soc->revision, soc->type);

	if (!strcmp("OMAP4430", soc->machine)) {
		if (!strcmp("ES1.", soc->revision)) {
			soc->id = OMAP4430_ES1;
			soc->package = OMAP_PACKAGE_CBL;
		} else {
			soc->id = OMAP4430_ES2;
			soc->package = OMAP_PACKAGE_CBS;
		}
	}
	if (!strcmp("OMAP4460", soc->machine)) {
		if (!strcmp("ES1.1", soc->revision)) {
			soc->id = OMAP4460_ES1_1;
			soc->package = OMAP_PACKAGE_CBS;
		}
	}

	return 0;
}

/* REVISIT: Add masks to support am335x and dra7 */
static void decode_signal(struct soc_info *soc,
			  struct padconf *padconf,
			  struct omap_mux *signals,
			  uint32_t pa, uint32_t val)
{
	uint32_t off_mode, active_mode, mux_mode;
	char buf[128];
	char *bufp = buf;
	char *name;

	off_mode = val & 0xfe00;
	active_mode = val & 0x1f8;
	mux_mode = val & 7;

	if (off_mode & WAKEUP_EN) {
		fprintf(stderr, "WARNING: Please use dedicated wakeirq instead of WAKEUP_EN for pin %x: %x (%x)\n",
			pa, off_mode, val);
		off_mode &= ~WAKEUP_EN;
	}

	switch (off_mode) {
	case PIN_OFF_NONE:
		break;
	case OFFOUT_EN:
		fprintf(stderr, "REVISIT: OFFOUT_EN without OFF_EN? Check pin %x: %x (%x)\n",
			pa, off_mode, val);
		bufp += sprintf(bufp, "OFFOUT_EN | ");
		break;
	case PIN_OFF_OUTPUT_HIGH:
		bufp += sprintf(bufp, "PIN_OFF_OUTPUT_HIGH | ");
		break;
	case PIN_OFF_OUTPUT_LOW:
		bufp += sprintf(bufp, "PIN_OFF_OUTPUT_LOW | ");
		break;
	case PIN_OFF_INPUT_PULLUP:
		bufp += sprintf(bufp, "PIN_OFF_INPUT_PULLUP | ");
		break;
	case PIN_OFF_INPUT_PULLDOWN:
		bufp += sprintf(bufp, "PIN_OFF_INPUT_PULLDOWN | ");
		break;
	default:
		bufp += sprintf(bufp, "OFF_MODE_UNKNOWN | ");
		fprintf(stderr, "WARNING: Unknown off mode val for pin %x: %x (%x)\n",
			pa, off_mode, val);
		break;
 	}

	switch (active_mode) {
	case PIN_OUTPUT_PULLUP:
		bufp += sprintf(bufp, "PIN_OUTPUT_PULLUP | ");
		break;
	case PIN_OUTPUT_PULLDOWN:
		bufp += sprintf(bufp, "PIN_OUTPUT_PULLDOWN | ");
		break;
	case PIN_OUTPUT:
		bufp += sprintf(bufp, "PIN_OUTPUT | ");
		break;
 	case PIN_INPUT:
		bufp += sprintf(bufp, "PIN_INPUT | ");
		break;
	case PIN_INPUT_PULLUP:
		bufp += sprintf(bufp, "PIN_INPUT_PULLUP | ");
		break;
	case PIN_INPUT_PULLDOWN:
		bufp += sprintf(bufp, "PIN_INPUT_PULLDOWN | ");
		break;
	default:
		bufp += sprintf(bufp, "ACTIVE_MODE_UNKNOWN | ");
		fprintf(stderr, "WARNING: Unknown active mode val for pin %x: %x (%x)\n",
			pa, active_mode, val);
		break;
	}

	bufp += sprintf(bufp, "MUX_MODE%i", mux_mode);

	/* Safe mode may not be always listed */
	if (mux_mode == 7 && !signals->muxnames[mux_mode])
		name = "safe_mode";
	else
		name = signals->muxnames[mux_mode];

	printf("\n\t\t\t/* 0x%x %s.%s pad %s */\n", pa,
	       signals->muxnames[0], name,
	       signals->balls[0]);
	printf("\t\t\t<OMAP4_IOPAD(0x%03x, %s)>", pa & 0x0fff, buf);
}

static int print_one_pin(struct soc_info *soc,
			 struct padconf *padconf,
			 uint32_t pa, uint32_t val)
{
	struct omap_mux *signals = padconf->signals;
	int found = 0;

	while (signals->reg_offset !=  OMAP_MUX_TERMINATOR) {
		uint32_t addr = padconf->mux_pbase + signals->reg_offset;

		if (addr == pa) {
			decode_signal(soc, padconf, signals, pa, val);
			found++;
			break;
		}
		signals++;
	}

	if (!found)
		fprintf(stderr, "WARNING: Could not find pin at %x, please add\n", pa);

	return 0;
}

static int parse_one_pinctrl(struct soc_info *soc,
			     struct padconf *padconf,
			     char *filename) {
	FILE *f;
	char line[1024];
	int i = 0, error = 0;

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "ERROR: Could not open file %s: %i\n", filename, -errno);

		return -errno;
	}

	printf("\tpinmux-%s-pool-pins {\n", padconf->name);
	printf("\t\tpinctrl-single,pins = ");

	while (fgets(line, sizeof(line), f)) {
		uint32_t pa, val;
		char *token;
		int error = 0;

		if (i > 1)
			printf(",\n");

		/* Ignore first line "registered pins" */
		if (i > 0) {

			/* Ignore pin, pin number, pin name and flags fields */
			token = strtok(line, " ");
			token = strtok(NULL, " ");
			token = strtok(NULL, " ");
			token = strtok(NULL, " ");

			/* Padconf register physical address */
			token = strtok(NULL, " ");
			pa = strtoul(token, NULL, 16);

			/* Padconf register value */
			token = strtok(NULL, " ");
			val = strtoul(token, NULL, 16);

			error = print_one_pin(soc, padconf, pa, val);
			if (error)
				break;
		}

		i++;
	}

	printf(";\n");

	printf("\t};\n\n");

	fclose(f);

	return error;
}

static int parse_pinctrl(struct soc_info *soc) {
	char filename[128];
	int i, error = 0;

	for (i = 0; i < MAX_PADCONF_NR; i++) {
		struct padconf *p;

		p = &soc->padconf[i];

		if (!p->name)
			break;

		sprintf(filename,
			"/sys/kernel/debug/pinctrl/%08x.pinmux-pinctrl-single/pins",
			p->mux_pbase + p->mux_offset);

		error = parse_one_pinctrl(soc, p, filename);
		if (error)
			break;

	}

	return error;
}

static void omap_mux_package_fixup(struct omap_mux *p,
				   struct omap_mux *superset)
{
	while (p->reg_offset !=  OMAP_MUX_TERMINATOR) {
		struct omap_mux *s = superset;
		int found = 0;

		while (s->reg_offset != OMAP_MUX_TERMINATOR) {
			if (s->reg_offset == p->reg_offset) {
				*s = *p;
				found++;
				break;
			}
			s++;
		}
		if (!found)
			fprintf(stderr, "ERROR: %s: Unknown entry offset 0x%x\n", __func__,
			       p->reg_offset);
		p++;
	}
}

static void omap_mux_package_init_balls(struct omap_ball *b,
					struct omap_mux *superset)
{
	while (b->reg_offset != OMAP_MUX_TERMINATOR) {
		struct omap_mux *s = superset;
		int found = 0;

		while (s->reg_offset != OMAP_MUX_TERMINATOR) {
			if (s->reg_offset == b->reg_offset) {
				s->balls[0] = b->balls[0];
				s->balls[1] = b->balls[1];
				found++;
				break;
			}
			s++;
		}
		if (!found)
			fprintf(stderr, "ERROR: %s: Unknown ball offset 0x%x\n", __func__,
			       b->reg_offset);
		b++;
	}
}

int omap_mux_init(const char *name, uint32_t flags,
		  uint32_t mux_pbase, uint32_t mux_size,
		  struct omap_mux *superset,
		  struct omap_mux *package_subset,
		  struct omap_board_mux *board_mux,
		  struct omap_ball *package_balls)
{
	struct padconf *padconf = NULL;
	int i;

	for (i = 0; i < MAX_PADCONF_NR; i++) {
		struct padconf *p;

		p = &soc.padconf[i];
		if (!p->name) {
			padconf = p;
			break;
		}
	}

	if (!padconf) {
		fprintf(stderr, "ERROR: Could not init padconf, check MAX_PADCONF_NR\n");

		return -EINVAL;
	}

	padconf->name = name;
	padconf->flags = flags;
	padconf->mux_pbase = mux_pbase;
	padconf->mux_offset = superset[0].reg_offset;
	padconf->mux_size = mux_size;

	if (package_subset)
		omap_mux_package_fixup(package_subset, superset);
	if (package_balls)
		omap_mux_package_init_balls(package_balls, superset);

	padconf->signals = superset;

	return 0;
}

int main(int argc, char** argv)
{
	int error;

	error = detect_system(&soc);
	if (error)
		return error;

	switch (soc.id) {
	case OMAP4430_ES1:
		error = omap4_mux_init(NULL, NULL, OMAP_PACKAGE_CBL);
		if (error)
			return error;
		break;
	case OMAP4430_ES2:
	case OMAP4460_ES1_1:
		error = omap4_mux_init(NULL, NULL, OMAP_PACKAGE_CBS);
		if (error)
			return error;
		break;
	default:
		fprintf(stderr, "ERROR: Unknown SoC or package, please add\n");
		return -EINVAL;
		break;
	}

	error = parse_pinctrl(&soc);
	if (error)
		return error;

	return 0;
}
