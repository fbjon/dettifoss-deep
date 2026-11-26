/* SPDX-License-Identifier: GPL-3.0-only */
/* OMNI platform on the FM-1: plat.h over Felucca's HAL, panel map, USB rings and storage.
 * Part of the unity build (main.c), after the HAL, usb.c, storage.c and panel.c. */

uint32_t plat_ms(void) { return fm1_ms; }
uint32_t plat_us(void) { return MQ_NOW_US(); }

/* one look at the panel: plat_buttons() takes it (the buttons and the keys held, no tap since the last look lost:
 * hal/fm1_taps.h), plat_keys() gives that look's keys. The UI calls them in this order, once each time round. */
static uint32_t look_keys;
static int rm_is_on(void);
uint32_t plat_buttons(void)
{
    uint32_t raw, out = 0, i;
    if (rm_is_on()) {                               /* the panel is the computer's: no hand on it here */
        look_keys = 0;
        return 0;
    }
    raw = fm1_input_held(&look_keys);
    for (i = 0; i < NB; i++)
        if (raw & (1u << panel.btn[i]))
            out |= 1u << i;
    return out;
}

int plat_headphones(void) { return fm1_jack_phones(); }
uint32_t plat_keys(void) { return look_keys & ((1u << NKEYS) - 1u); }

int32_t plat_enc(int role)
{
    if (role < 0 || role >= NE || rm_is_on())
        return 0;
    return fm1_enc_take(panel.enc[role]) * panel.dir[role];
}

static volatile uint32_t master_q12 = 2048, master_k10;   /* the level, and the pot's turn 0 .. 1023 */

/* REMOTE: the panel played into the emulator on a computer (web/emu), for trying a build without flashing it.
 * The page sends its lights over USB-MIDI, when they change and at least twice a second; while it does, the FM-1's own
 * sound and screen rest (the UI sees no hand on the panel, MASTER at 0), the keys and buttons show the page's lights,
 * and every change on the panel goes to the page. Eight seconds without hearing it, the FM-1 is itself again. One kind
 * of frame keeps it there because the FM-1 holds one SysEx at a time and drops what comes meanwhile: the page leaves a
 * frame between its sends. SysEx F0 7D 44 44 cmd .. F7 ("DD"):
 *   page -> FM-1   2 BYE            stop
 *                  3 LIGHTS l..     41 levels 0..127: the buttons (B_*), then the keys from F3
 *   FM-1 -> page   1 HELLO ver      the answer to the first LIGHTS, and again each second
 *                  16 PANEL         the buttons held (B_*, 2 x 7 bits), the keys (4 x 7 bits from F3), MASTER's
 *                                   turn (2 x 7 bits, 0 .. 1023), and each knob's detents since the last (EN_*, 64 + n) */
#define RM_GONE_MS 8000u
static struct {
    uint8_t on, sent;
    uint32_t heard, said, btn, keys, master;
    int32_t enc[NE];
    uint8_t lb[NB], lk[NKEYS];
} rm;
static int rm_frame(const uint8_t *p, uint32_t n)   /* a SysEx from the computer (between F0 and F7): 1 if ours */
{
    uint32_t i;
    if (n < 4u || p[0] != 0x7Du || p[1] != 0x44u || p[2] != 0x44u)
        return 0;
    if (p[3] == 2u && rm.on) {
        rm.heard = fm1_ms - RM_GONE_MS - 1u;
    } else if (p[3] == 3u && n >= 4u + NB + NKEYS) {
        static const uint8_t HI[] = {0xF0, 0x7D, 0x44, 0x44, 1, 1, 0xF7};
        if (!rm.on) {
            memset(&rm, 0, sizeof rm);
            rm.on = 1;
            rm.said = fm1_ms - 1000u;
            ui_remote(1);
        }
        rm.heard = fm1_ms;
        if (fm1_ms - rm.said >= 1000u && !ann_wire_send(HI, sizeof HI))
            rm.said = fm1_ms;
        for (i = 0; i < NB; i++)
            rm.lb[i] = (uint8_t)(p[4 + i] * 2u + (p[4 + i] >> 6));
        for (i = 0; i < NKEYS; i++)
            rm.lk[i] = (uint8_t)(p[4 + NB + i] * 2u + (p[4 + NB + i] >> 6));
    }
    return 1;
}
static void rm_poll(void)                           /* from the main loop: the panel to the page */
{
    uint32_t raw, k, b = 0, i;
    int moved = 0;
    if (!rm.on)
        return;
    if (fm1_ms - rm.heard > RM_GONE_MS) {
        rm.on = 0;
        ui_remote(0);
        return;
    }
    raw = fm1_input_held(&k);
    for (i = 0; i < NB; i++)
        if (raw & (1u << panel.btn[i]))
            b |= 1u << i;
    k &= (1u << NKEYS) - 1u;
    for (i = 0; i < NE; i++) {
        rm.enc[i] += fm1_enc_take(panel.enc[i]) * panel.dir[i];
        moved |= rm.enc[i] != 0;
    }
    if (master_k10 + 4u < rm.master || master_k10 > rm.master + 4u || (master_k10 != rm.master && !master_k10) ||
        (master_k10 != rm.master && master_k10 == 1023u))
        moved = 1;                                  /* the pot, past its jitter (and always to its ends) */
    if (rm.sent && b == rm.btn && k == rm.keys && !moved)
        return;
    {
        uint8_t f[4 + 1 + 2 + 4 + 2 + NE + 1] = {0xF0, 0x7D, 0x44, 0x44, 16};
        const uint32_t m = master_k10;
        f[5] = (uint8_t)(b & 127u), f[6] = (uint8_t)(b >> 7 & 127u);
        for (i = 0; i < 4u; i++)
            f[7 + i] = (uint8_t)(k >> (7u * i) & 127u);
        f[11] = (uint8_t)(m & 127u), f[12] = (uint8_t)(m >> 7 & 127u);
        for (i = 0; i < NE; i++)
            f[13 + i] = (uint8_t)(64 + (rm.enc[i] > 63 ? 63 : rm.enc[i] < -63 ? -63 : rm.enc[i]));
        f[sizeof f - 1u] = 0xF7;
        if (ann_wire_send(f, sizeof f))
            return;                                 /* no room: the same, with what moved since, next time */
        rm.sent = 1, rm.btn = b, rm.keys = k, rm.master = m;
        for (i = 0; i < NE; i++)
            rm.enc[i] -= f[13 + i] - 64;
    }
}

static int rm_is_on(void) { return rm.on; }
uint32_t plat_master(void) { return rm.on ? 0u : master_q12; }

/* LEDs: the picture is built off-line and copied one byte per column (Felucca: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker) */
static uint8_t led_pos[FM1_NKEY];
static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < FM1_NKEY; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
/* a level 0..255 as the eye sees it; 255 is steadily lit, below that the HAL's PWM: the share of frames lit
 * is the level squared (the eye's curve, roughly), at least one frame in 32 for any level above 0.
 * Moving (a gesture coming in, a scene's key filling), a light lands between two of the 32 steps by taking the
 * one above as often as it's nearer it (the remainder carried from one UI frame to the next), so it fades
 * rather than steps, from dark; still for LED_STILL frames, it rounds, so a steady light never shimmers. In the
 * UI's frame, never the audio's: a few adds a light, 60 times a second at most */
#define LED_STILL 8u
static uint8_t led_last[NB + NKEYS], led_moving[NB + NKEYS], led_carry[NB + NKEYS];
void plat_leds(const uint8_t *buttons, const uint8_t *keys)
{
    uint8_t nl[FM1_NCOL] = {0}, pw[FM1_LED_PHASES][FM1_NCOL];
    uint32_t i, c, f;
    if (rm.on)                                      /* the emulator's lights */
        buttons = rm.lb, keys = rm.lk;
    memset(pw, 0, sizeof pw);
    for (i = 0; i < NB + NKEYS; i++) {
        uint32_t id = i < NB ? panel.btn[i] : 14u + (i - NB), lv = i < NB ? buttons[i] : keys[i - NB], d;
        uint8_t q = led_pos[id], m;
        if (lv != led_last[i]) {
            led_last[i] = (uint8_t)lv;
            led_moving[i] = LED_STILL;
        } else if (led_moving[i])
            led_moving[i]--;
        if (!lv || q == 0xFF)
            continue;
        m = (uint8_t)(1u << (q & 7u));
        if (lv >= 255u) {
            nl[q >> 3] |= m;
            continue;
        }
        d = lv * lv * FM1_LED_PHASES * 256u / (255u * 255u);   /* frames lit, x 256 */
        if (led_moving[i]) {
            uint32_t r = led_carry[i] + (d & 255u);
            led_carry[i] = (uint8_t)r;
            d = (d >> 8) + (r >> 8);
            if (!d)
                continue;                              /* under one frame: lit only every so often */
        } else {
            led_carry[i] = 0;
            d = (d + 128u) >> 8;
            if (!d)
                d = 1;
        }
        for (f = 0; f < FM1_LED_PHASES; f++)       /* d frames of 32, evenly spread */
            if ((f + 1u) * d / FM1_LED_PHASES != f * d / FM1_LED_PHASES)
                pw[f][q >> 3] |= m;
    }
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
    for (f = 0; f < FM1_LED_PHASES; f++)
        for (c = 0; c < FM1_NCOL; c++)
            fm1_led_pwm[f][c] = pw[f][c];
}

/* KEY PRESSURE (aftertouch), MONK-1's (Charles Vestal), from Velo's sensing (sense.c). The scan tick hands
 * each sample of the two analog rows (PA7, PA8: the 20 lower keys, F3 .. C5) to sense.c; the seven top keys
 * (C#5 .. G5, row PA6) have no converter and no pressure. Nothing plays from it yet: the digital scan still
 * says which keys are down. */
static int plat_analog_led_ok(uint32_t col, uint32_t rowbit)   /* a held key: its column's LEDs dark while it's read */
{
    int id = FM1_KEYMAP[rowbit][col];
    uint32_t k = id >= 14 ? (uint32_t)id - 14u : 0xFFu;
    return k >= NKEYS || sn_key[k].st == SN_OPEN;
}
static void plat_analog_sample(uint32_t col, uint32_t rowbit, uint32_t v, uint32_t driven)
{
    int id = FM1_KEYMAP[rowbit][col];
    if (id >= 14)
        sense_sample((uint32_t)id - 14u, v, fm1_ticks(), (int)driven);
}
static int plat_analog_drive(uint32_t col, uint32_t rowbit)
{
    int id = FM1_KEYMAP[rowbit][col];
    return id >= 14 && sense_want_drive((uint32_t)id - 14u);
}
static uint32_t plat_analog_mask(void)
{
    uint32_t m = 0, c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 3; r <= 4u; r++)
            if (FM1_KEYMAP[r][c] >= 14)
                m |= 1u << (FM1_KEYMAP[r][c] - 14);
    return m;
}
/* Velo's settings as MONK-1 ships them: the driven reading at DRIVE 2, the deeper of two samples (WAIT 2),
 * no calibration; converter divider 7, delay 2; no top-row probe (it gives a velocity, never a pressure) */
static void plat_pressure_init(void)
{
    sense_init(plat_analog_mask());
    sn_cfg.touch = 900;
    sn_cfg.press = 400;
    sn_cfg.release = 600;
    sn_cfg.wait = 2;
    sn_cfg.v_lo = 0;
    sn_cfg.v_hi = 72;
    sn_cfg.vmax = 127;
    sn_cfg.at_range = 32;
    sn_cfg.drive = 2;
    sn_cfg.d_curve = 70;
    sn_cfg.d_ceil = 896;
    sn_cfg.d_at_range = 64;
    sn_cfg.learn = 0;
    sense_cal_reset();
    fm1_an.immediate = 0;
    fm1_an.drive = 2;
    fm1_an.probe = 0;
    fm1_an.con = (uint16_t)FM1_ADC_CON(7, 2);
}
/* The pressure on an absolute scale (v0.2: MONK-1's, counted from where the key rests, drifted after a held
 * press and read a fast hard press as 0, on Evan's FM-1): the pill's resistance (the driven reading less the
 * diode, SN_DBASE) on a log scale, conductive rubber's resistance falling as a power of the force: 0 at
 * PR_LIGHT (a light touch that seats), 100 % at PR_FIRM (a firm press). Smoothed over ~30 ms. Every 5 ms
 * from the main loop. For measuring, over USB: the pressure as polyphonic aftertouch (the key as a note,
 * F3 = 53), and the raw driven reading / 8 as CC (the key's number, 0..26) on channel 16; both 0 when let go. */
/* driven readings, measured on Evan's FM-1 (2026-10-16, tools/fm1_pressure.py, F3 B3 E4 G4): a light touch
 * 920 .. 1016, the firmest press 480 .. 528. 0 % under a resting finger, 100 % within every key's firm press */
#define PR_LIGHT 900.0f
#define PR_FIRM 540.0f
/* Evan, on the FM-1: a light press read too high, and holding a firm press the number fell: the reading creeps
 * back up ~30 counts under a steady hard press (472 .. 504, measured). So a curve (x ^ PR_CURVE: slow at first,
 * steep near the top) and a hysteresis: harder is followed at once, easing only once the reading has risen
 * PR_HYST above the firmest point since (the creep and the jitter stay put; a real ease-off still moves it) */
#define PR_CURVE 1.6f
#define PR_HYST 32u
static float p_s[NKEYS];
static uint16_t p_ref[NKEYS];
static uint8_t p_val[NKEYS], p_raw[NKEYS];
static void plat_pressure_poll(void)
{
    static uint32_t t;
    const float lo = PR_FIRM - (float)SN_DBASE, hi = PR_LIGHT - (float)SN_DBASE;
    uint32_t i, now = fm1_ms;
    if (now - t < 5u)
        return;
    t = now;
    for (i = 0; i < NKEYS; i++) {
        float r, x;
        uint8_t p, raw;
        if (!sense_is_analog(i))
            continue;
        if (sn_key[i].st != SN_DOWN || !sn_key[i].dn) {
            if (p_raw[i]) {                                    /* let go: both to 0 (a held key's raw never is) */
                plat_midi_out(0x0Au | 0xA0u << 8 | (53u + i) << 16);
                plat_midi_out(0x0Bu | 0xBFu << 8 | i << 16);
            }
            p_s[i] = 0.0f;
            p_val[i] = 0;
            p_raw[i] = 0;
            p_ref[i] = 0;
            continue;
        }
        if (!p_ref[i] || sn_key[i].d < p_ref[i])
            p_ref[i] = sn_key[i].d;                            /* harder: at once */
        else if (sn_key[i].d > p_ref[i] + PR_HYST)
            p_ref[i] = (uint16_t)(sn_key[i].d - PR_HYST);      /* eased off, past the creep */
        r = (float)(p_ref[i] > SN_DBASE + 1u ? p_ref[i] - SN_DBASE : 1u);
        x = fm_clampf(fm_log2f(hi / r) / fm_log2f(hi / lo), 0.0f, 1.0f);
        x = fm_exp2f(PR_CURVE * fm_log2f(x > 1e-6f ? x : 1e-6f));   /* x ^ PR_CURVE */
        p_s[i] += (x - p_s[i]) * 0.17f;
        p = (uint8_t)(p_s[i] * 127.0f + 0.5f);
        raw = (uint8_t)(sn_key[i].d >> 3 > 127u ? 127u : sn_key[i].d >> 3);
        if (p != p_val[i]) {
            p_val[i] = p;
            plat_midi_out(0x0Au | 0xA0u << 8 | (53u + i) << 16 | (uint32_t)p << 24);
        }
        if (raw != p_raw[i]) {
            p_raw[i] = raw;
            plat_midi_out(0x0Bu | 0xBFu << 8 | i << 16 | (uint32_t)raw << 24);
        }
    }
}
int plat_pressure(uint32_t k) { return k < NKEYS && sense_is_analog(k) ? p_val[k] : -1; }

/* MIDI: Felucca's rings carry 4-byte USB-MIDI event packets, byte 0 = cable / CIN */
int plat_midi_in(uint32_t *pkt, uint32_t *us)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = midi_in_q[mi_r % MQ];
    *us = midi_in_t[mi_r % MQ];
    mi_r++;
    return 1;
}
void plat_midi_out(uint32_t pkt) { midi_out_event(pkt); }

int plat_store_load(uint32_t obj, void *dst, uint32_t max) { return flash_ok ? st_load(obj, dst, max) : -1; }
void *plat_store_buf(void) { return st_buf; }
_Static_assert(sizeof st_buf >= PLAT_STORE_MAX, "plat_store_buf: an object fits the store's buffer");
static uint8_t st_hold;                         /* a restore staged (main_fm1.c): no saves till it's done (they'd
                                                 * write over the staged copies) */
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    return !flash_ok ? -9 : st_hold ? -10 : st_save(obj, src, len);
}
int plat_store_held(void) { return st_hold; }

static volatile uint32_t audio_cpu_pct, audio_xruns;
uint32_t plat_cpu_pct(void) { return audio_cpu_pct; }
uint32_t plat_xruns(void) { return audio_xruns; }
