#include "event.h"

#include "classic.h"
#include "guitar_hero_3.h"
#include "ir.h"
#include "motion_plus.h"
#include "nunchuk.h"
#include "wii_board.h"

#define ABS(x) ((s32)(x) > 0 ? (s32)(x) : -((s32)(x)))

#define CHECK_THRESHOLD(thresh, a, b) \
    (((thresh) > WPAD_THRESH_IGNORE) && (ABS((a) - (b)) > (thresh)))

#define CHECK_THRESHOLD_SIMPLE(thresh, a, b) \
    (((thresh) > WPAD_THRESH_IGNORE) && ((a) != (b)))

static bool check_threshold_js(
	int threshold, const struct joystick_t *new, const struct joystick_t *old)
{
	if (CHECK_THRESHOLD(threshold, new->pos.x, old->pos.x)) return true;
	if (CHECK_THRESHOLD(threshold, new->pos.y, old->pos.y)) return true;
	return false;
}

static bool check_threshold_accel(
	int threshold, const struct vec3w_t *new, const struct vec3w_t *old)
{
	if (CHECK_THRESHOLD(threshold, new->x, old->x)) return true;
	if (CHECK_THRESHOLD(threshold, new->y, old->y)) return true;
	if (CHECK_THRESHOLD(threshold, new->z, old->z)) return true;
	return false;
}

static bool check_threshold_ir(
	int threshold, const WPADData *new, const WPADData *old)
{
	for (int i = 0; i < WPAD_MAX_IR_DOTS; i++) {
		if (new->ir.dot[i].visible != old->ir.dot[i].visible) return true;
		if (CHECK_THRESHOLD(threshold, new->ir.dot[i].rx, old->ir.dot[i].rx)) return true;
		if (CHECK_THRESHOLD(threshold, new->ir.dot[i].ry, old->ir.dot[i].ry)) return true;
	}
	return false;
}

static bool handle_expansion(const WPADData *wpad_info, const uint8_t *msg,
                             const WpadThresholds *thresh, WPADData *out)
{
	WPAD2_DEBUG("exp type: %d", wpad_info->exp.type);
	bool changed = false;
	switch (out->exp.type) {
	case EXP_NUNCHUK:
		struct nunchuk_t *nc = &out->exp.nunchuk;
		const struct nunchuk_t *nc_info = &wpad_info->exp.nunchuk;
		_wpad2_nunchuk_event(nc, msg);
		if (thresh &&
		    (CHECK_THRESHOLD_SIMPLE(thresh->btns, nc->btns, nc_info->btns) ||
		     check_threshold_js(thresh->js, &nc->js, &nc_info->js) ||
		     check_threshold_accel(thresh->acc, &nc->accel, &nc_info->accel))) {
			changed = true;
		}
		break;
	case EXP_CLASSIC:
		struct classic_ctrl_t *cc = &out->exp.classic;
		const struct classic_ctrl_t *cc_info = &wpad_info->exp.classic;
		cc->type = wpad_info->exp.classic.type;
		_wpad2_classic_event(cc, msg);
		if (thresh &&
		    (CHECK_THRESHOLD_SIMPLE(thresh->btns, cc->btns, cc_info->btns) ||
		     check_threshold_js(thresh->js, &cc->ljs, &cc_info->ljs) ||
		     check_threshold_js(thresh->js, &cc->rjs, &cc_info->rjs) ||
		     CHECK_THRESHOLD(thresh->js, cc->rs_raw, cc_info->rs_raw) ||
		     CHECK_THRESHOLD(thresh->js, cc->ls_raw, cc_info->ls_raw))) {
			changed = true;
		}
		break;
	case EXP_GUITAR_HERO_3:
		struct guitar_hero_3_t *gh = &out->exp.gh3;
		const struct guitar_hero_3_t *gh_info = &wpad_info->exp.gh3;
		_wpad2_guitar_hero_3_event(gh, msg);
		if (thresh &&
		    (CHECK_THRESHOLD_SIMPLE(thresh->btns, gh->btns, gh_info->btns) ||
		     check_threshold_js(thresh->js, &gh->js, &gh_info->js) ||
		     CHECK_THRESHOLD(thresh->js, gh->wb_raw, gh_info->wb_raw))) {
			changed = true;
		}
		break;
	case EXP_WII_BOARD:
		struct wii_board_t *wb = &out->exp.wb;
		const struct wii_board_t *wb_info = &wpad_info->exp.wb;
		_wpad2_wii_board_event(wb, msg);
		if (thresh &&
		    (CHECK_THRESHOLD(thresh->wb, wb->rtl, wb_info->rtl) ||
		     CHECK_THRESHOLD(thresh->wb, wb->rtr, wb_info->rtr) ||
		     CHECK_THRESHOLD(thresh->wb, wb->rbl, wb_info->rbl) ||
		     CHECK_THRESHOLD(thresh->wb, wb->rbr, wb_info->rbr))) {
			changed = true;
		}
		break;
	case EXP_MOTION_PLUS:
		struct motion_plus_t *mp = &out->exp.mp;
		const struct motion_plus_t *mp_info = &wpad_info->exp.mp;
		_wpad2_motion_plus_event(mp, msg);
		if (thresh &&
		    (CHECK_THRESHOLD(thresh->mp, mp->rx, mp_info->rx) ||
		     CHECK_THRESHOLD(thresh->mp, mp->ry, mp_info->ry) ||
		     CHECK_THRESHOLD(thresh->mp, mp->rz, mp_info->rz))) {
			changed = true;
		}
		break;
	default:
		return false;
	}
	out->data_present |= WPAD_DATA_EXPANSION;
	return changed;
}

static bool pressed_buttons(const WPADData *wpad_info, const uint8_t *msg,
                            const WpadThresholds *thresh, WPADData *out)
{
	uint16_t now = read_be16(msg) & WIIMOTE_BUTTON_ALL;

	/* buttons pressed now */
	out->btns_h = now;
	out->data_present |= WPAD_DATA_BUTTONS;
	WPAD2_DEBUG("Buttons %04x", now);
	return CHECK_THRESHOLD_SIMPLE(thresh->btns, out->btns_h, wpad_info->btns_h);
}

static bool parse_accel(const WPADData *wpad_info, const uint8_t *msg,
                        const WpadThresholds *thresh, WPADData *out)
{
	out->accel.x = (msg[2] << 2) | ((msg[0] >> 5) & 3);
	out->accel.y = (msg[3] << 2) | ((msg[1] >> 4) & 2);
	out->accel.z = (msg[4] << 2) | ((msg[1] >> 5) & 2);
	out->data_present |= WPAD_DATA_ACCEL;
	return thresh && check_threshold_accel(thresh->acc, &out->accel, &wpad_info->accel);
}

/* Return true if the data has changed (taking the thresholds into account) */
bool _wpad2_event_parse_report(const WPADData *wpad_info, const uint8_t *data,
                               const WpadThresholds *thresh, WPADData *out)
{
	uint8_t event = data[0];
	const uint8_t *msg = data + 1;

	out->data_present = 0;
	out->exp = wpad_info->exp;
	/* We set the "thresh" pointer to NULL if some data has changed, to avoid
	 * doing useless threshold checks. If "thresh" is NULL at the end of this
	 * function, it means that some data has significantly changed. */
	switch (event) {
	case WM_RPT_BTN:
		if (pressed_buttons(wpad_info, msg, thresh, out)) thresh = NULL;
		break;
	case WM_RPT_BTN_ACC:
		if (pressed_buttons(wpad_info, msg, thresh, out)) thresh = NULL;
		if (parse_accel(wpad_info, msg, thresh, out)) thresh = NULL;
		break;
	case WM_RPT_BTN_ACC_IR:
		if (pressed_buttons(wpad_info, msg, thresh, out)) thresh = NULL;
		if (parse_accel(wpad_info, msg, thresh, out)) thresh = NULL;
		_wpad2_ir_parse_extended(out, msg + 5);
		if (thresh && check_threshold_ir(thresh->ir, out, wpad_info)) thresh = NULL;
		break;
	case WM_RPT_BTN_EXP:
		if (pressed_buttons(wpad_info, msg, thresh, out)) thresh = NULL;
		if (handle_expansion(wpad_info, msg + 2, thresh, out)) thresh = NULL;
		break;
	case WM_RPT_BTN_ACC_EXP:
		/* button - motion - expansion */
		if (pressed_buttons(wpad_info, msg, thresh, out)) thresh = NULL;
		if (parse_accel(wpad_info, msg, thresh, out)) thresh = NULL;
		if (handle_expansion(wpad_info, msg + 5, thresh, out)) thresh = NULL;
		break;
	case WM_RPT_BTN_IR_EXP:
		if (pressed_buttons(wpad_info, msg, thresh, out)) thresh = NULL;
		_wpad2_ir_parse_basic(out, msg + 2);
		if (thresh && check_threshold_ir(thresh->ir, out, wpad_info)) thresh = NULL;
		if (handle_expansion(wpad_info, msg + 12, thresh, out)) thresh = NULL;
		break;
	case WM_RPT_BTN_ACC_IR_EXP:
		/* button - motion - ir - expansion */
		if (pressed_buttons(wpad_info, msg, thresh, out)) thresh = NULL;
		if (parse_accel(wpad_info, msg, thresh, out)) thresh = NULL;

		/* ir */
		_wpad2_ir_parse_basic(out, msg + 5);
		if (thresh && check_threshold_ir(thresh->ir, out, wpad_info)) thresh = NULL;

		if (handle_expansion(wpad_info, msg + 15, thresh, out)) thresh = NULL;
		break;
	default:
		WPAD2_WARNING("Unknown event, can not handle it [Code 0x%02x].", event);
		return false;
	}
	return thresh == NULL;
}
