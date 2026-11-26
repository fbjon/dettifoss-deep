/* SPDX-License-Identifier: GPL-3.0-only */
/* THE LIVE PAGE (HOME, where the FM-1 starts; HOME again: the progression, and back): chords on the keys, played with
 * one hand, over the drone.
 *
 *   F3 .. E4       the twelve roots, latching: a tap plays that chord, its key lit while it sounds
 *   F#4 .. F#5     the modifiers, on the black keys above: Minor, Sus2, Sus4, 7, Maj7, Add9. A tap arms or
 *                  disarms one (lit while armed); the next root plays with them, and tapping the lit root applies
 *                  them now. Sus2 and Sus4 exclude each other, as 7 and Maj7 do; a sus wins over Minor
 *   G5             BASS: a tap arms it (blinking), then a root key sets the bass apart from the chord's root (a
 *                  slash chord); the bass holds under chord changes until the lit Bass is tapped again
 *   SEL + the lit root   the chord let go: the drones back to the sound's own pitches
 *   OCT- / OCT+    the chords an octave down / up (-1 .. +1)
 *   KNOB 1-4       Dark-Bright, Thickness, Motion, Space (the live macros, a sound's)
 *   ALGORITHM      Snap: how a change lands: a wash (a slow glide) .. legato (a short one) .. a pad's attack
 *
 * A chord is a bass and three tones over it: drone I's main note the bass, its partner the fifth, drone II's main
 * the third (or the sus), its partner the extension (or the root two octaves up), so Thickness, turned down, takes the
 * extension away first, then the third, then the fifth. Dark-Bright past +-50 moves the upper two tones an octave
 * (back inside +-40).
 *
 * THE PROGRESSION (HOME on the Live page; HOME again: the chords): a progression's chords stepped through, the keys
 * above free for a lead (not yet):
 *
 *   F3 / F#3       the next / the previous chord, both always lit, taking turns at full each 0.5 s. Two ways
 *                  (SEL + F3): REST: a tap steps; held, the chord is released, and letting go brings in the next (air between).
 *                  HOLD: held, the chord plays; let go, it's released, and the next hold brings in the next.
 *                  While a chord rests the drones move to the next one's notes, so it comes in on them
 *   OCT, the knobs, ALGORITHM   as on the Live page
 *
 * SEL + HOME, on either, opens Niagara Drift's Drones and Pitch pages (again: the other), till the sound has its own.
 *
 * For now the progression is the demo's: C, Am, Fmaj7, G7, C. Part of ui.c's unity build (after look.c) */

enum { LM_MIN, LM_SUS2, LM_SUS4, LM_7, LM_MAJ7, LM_ADD9, LM_N };
static const char *const LV_MOD_NAME[LM_N] = {"m", "sus2", "sus4", "7", "maj7", "add9"};
static const uint8_t LV_MOD_K[LM_N] = {13, 15, 17, 20, 22, 25};   /* F#4 G#4 A#4 C#5 D#5 F#5 */
static const char *const LV_PC[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
#define LV_BASS_K 26                           /* G5 */
#define LV_NROOT 12                            /* F3 .. E4, keys 0 .. 11 */
#define LV_LOW 41                              /* F3's root: its bass F2 (MIDI) */
#define PG_NEXT 0                              /* F3 */
#define PG_PREV 1                              /* F#3 */
typedef struct { int8_t root; uint8_t mods; } pg_chord_t;
static const pg_chord_t PG_DEMO[] = {{7, 0}, {4, 1u << LM_MIN}, {0, 1u << LM_MAJ7}, {2, 1u << LM_7}, {7, 0}};
#define PG_N ((int)(sizeof PG_DEMO / sizeof PG_DEMO[0]))

static struct {
    int8_t root;                               /* the chord sounding: its root key (0 .. 11), or -1 */
    uint8_t mods;                              /* the modifiers armed (bit LM_*) */
    uint8_t smods;                             /* ... the chord sounding's */
    int8_t bass;                               /* the bass set apart: its root key, or -1 (the chord's root) */
    uint8_t bass_arm;                          /* Bass tapped: the next root key sets the bass */
    int8_t oct;                                /* the register, octaves */
    int8_t reg;                                /* Dark-Bright's octave for the upper tones now: -1, 0, 1 */
    uint8_t snap;
    int16_t note[4];                           /* the chord's notes as sent, MIDI: bass, fifth, third, extension */
    uint8_t prog;                              /* the progression's page */
    uint8_t hold;                              /* ... its way: 0 REST (held, released), 1 HOLD (held, played) */
    int8_t step;                               /* the step sounding (or last sounded), -1 none yet */
    int8_t to;                                 /* the step a held key goes to, -1 none */
    uint8_t rest;                              /* the chord released */
} lv = {-1, 0, 0, -1, 0, 0, 0, 50, {0, 0, 0, 0}, 0, 0, -1, -1, 0};

static char *lv_end(char *t)
{
    while (*t)
        t++;
    return t;
}
static void scat(char *t, const char *s) { scopy(lv_end(t), s); }
static int lv_third(uint8_t m) { return m >> LM_SUS2 & 1u ? 2 : m >> LM_SUS4 & 1u ? 5 : m >> LM_MIN & 1u ? 3 : 4; }
static int lv_ext(uint8_t m) { return m >> LM_MAJ7 & 1u ? 11 : m >> LM_7 & 1u ? 10 : m >> LM_ADD9 & 1u ? 14 : 24; }

static void lv_name(int root, uint8_t m, int bass, char *t)   /* "Am7", "Csus4/G", "Fmaj7" */
{
    const int pc = (5 + root) % 12;
    scopy(t, LV_PC[pc]);
    if ((m >> LM_MIN & 1u) && !(m & (1u << LM_SUS2 | 1u << LM_SUS4)))
        scat(t, "m");
    if (m >> LM_MAJ7 & 1u)
        scat(t, "maj7");
    else if (m >> LM_7 & 1u)
        scat(t, "7");
    if (m >> LM_SUS2 & 1u)
        scat(t, "sus2");
    else if (m >> LM_SUS4 & 1u)
        scat(t, "sus4");
    if ((m >> LM_ADD9 & 1u) && !(m & (1u << LM_7 | 1u << LM_MAJ7)))
        scat(t, "add9");
    if (bass >= 0 && bass != root) {
        scat(t, "/");
        scat(t, LV_PC[(5 + bass) % 12]);
    }
}

static void lv_notes(void)                     /* the chord's notes to the drones (taken up by dr_chord_go / a rest) */
{
    const int lo = LV_LOW + lv.root + 12 * lv.oct, up = 12 * lv.reg;
    int b = lv.bass >= 0 ? LV_LOW + lv.bass + 12 * lv.oct : lo;
    if (b > lo)                                /* a bass set apart goes under the chord */
        b -= 12;
    lv.note[0] = (int16_t)b;
    lv.note[1] = (int16_t)(lo + 7);
    lv.note[2] = (int16_t)(lo + 12 + lv_third(lv.smods) + up);
    lv.note[3] = (int16_t)(lo + 12 + lv_ext(lv.smods) + up);
    dr_chord(0, lv.note[0] * 100, (lv.note[1] - lv.note[0]) * 100, 1);
    dr_chord(1, lv.note[2] * 100, (lv.note[3] - lv.note[2]) * 100, 1);
}
static void lv_send(void)                      /* the chord sounding to the drones, as Snap says */
{
    lv_notes();
    dr_chord_go(1, lv.snap);
    lv.rest = 0;
}

static void pg_load(int i)                     /* step i as the chord (its notes sent, not yet taken up) */
{
    lv.root = PG_DEMO[i].root;
    lv.smods = PG_DEMO[i].mods;
    lv.bass = -1;
    lv_notes();
}
static int pg_after(int dir) { return lv.step < 0 ? 0 : (lv.step + dir + PG_N) % PG_N; }
static void pg_go(int i)                       /* step i in */
{
    lv.step = (int8_t)i;
    pg_load(i);
    dr_chord_go(1, lv.snap);
    lv.rest = 0;
}
static int live_prog(void) { return lv.prog; }
static void pg_rest(int next)                  /* released, the drones moving to step next's notes */
{
    int16_t was[4];
    int i;
    if (lv.root < 0 || lv.rest)
        return;
    for (i = 0; i < 4; i++)
        was[i] = lv.note[i];
    pg_load(next);
    for (i = 0; i < 4; i++)                    /* (the screen still shows the chord resting) */
        lv.note[i] = was[i];
    lv.root = PG_DEMO[lv.step].root;
    lv.smods = PG_DEMO[lv.step].mods;
    dr_chord_rest(lv.snap);
    lv.rest = 1;
}
static void pg_keys(uint32_t keys, uint32_t ch)
{
    int k;
    for (k = PG_NEXT; k <= PG_PREV; k++) {
        const int dir = k == PG_NEXT ? 1 : -1;
        if (!(ch >> k & 1u))
            continue;
        if (keys >> k & 1u) {                  /* the press */
            if (ui.btn >> B_SEL & 1u) {        /* SEL + F3: the way */
                ui.btn_used |= 1u << B_SEL;
                if (k == PG_NEXT) {
                    lv.hold ^= 1u;
                    say("PROGRESSION", lv.hold ? "HOLD TO PLAY" : "HOLD TO REST");
                }
                continue;
            }
            lv.to = (int8_t)pg_after(dir);
            if (lv.hold)
                pg_go(lv.to);
            else
                pg_rest(lv.to);
        } else if (lv.to >= 0) {               /* let go */
            if (lv.hold) {
                lv.to = -1;
                pg_rest(pg_after(dir));
            } else {
                pg_go(lv.to);
                lv.to = -1;
            }
        }
    }
}

static void live_home(void)                    /* HOME: back to the Live page as it was, and on it the other */
{
    lv.to = -1;
    if (ui.view == V_LIVE)
        lv.prog ^= 1u;
    else
        set_view(V_LIVE);
    if (lv.prog)
        say("PROGRESSION", lv.hold ? "HOLD TO PLAY" : "HOLD TO REST");
    else
        say("LIVE", "CHORDS ON THE KEYS");
}

static void live_keys(uint32_t keys, uint32_t ch)   /* the keys, on the press */
{
    const uint32_t dn = ch & keys;
    int k;
    if (lv.prog) {
        pg_keys(keys, ch);
        return;
    }
    for (k = 0; k < LV_NROOT; k++) {
        if (!(dn >> k & 1u))
            continue;
        if (ui.btn >> B_SEL & 1u) {             /* SEL + the lit root: the chord let go */
            ui.btn_used |= 1u << B_SEL;
            if (k == lv.root) {
                lv.root = -1;
                dr_chord_go(0, lv.snap);
            }
            continue;
        }
        if (lv.bass_arm) {                     /* Bass armed: this key the bass */
            lv.bass = (int8_t)k;
            lv.bass_arm = 0;
            if (lv.root < 0) {
                lv.root = (int8_t)k;
                lv.smods = lv.mods;
            }
        } else {
            lv.root = (int8_t)k;
            lv.smods = lv.mods;
        }
        lv_send();
    }
    for (k = 0; k < LM_N; k++) {
        if (!(dn >> LV_MOD_K[k] & 1u))
            continue;
        lv.mods ^= (uint8_t)(1u << k);
        if (lv.mods >> k & 1u) {               /* the pairs that exclude each other */
            if (k == LM_SUS2 || k == LM_SUS4)
                lv.mods &= (uint8_t)~(1u << (k == LM_SUS2 ? LM_SUS4 : LM_SUS2));
            if (k == LM_7 || k == LM_MAJ7)
                lv.mods &= (uint8_t)~(1u << (k == LM_7 ? LM_MAJ7 : LM_7));
        }
    }
    if (dn >> LV_BASS_K & 1u) {
        if (lv.bass >= 0) {                    /* the lit Bass: let go, the bass the root again */
            lv.bass = -1;
            lv.bass_arm = 0;
            if (lv.root >= 0)
                lv_send();
        } else
            lv.bass_arm = (uint8_t)!lv.bass_arm;
    }
}

static void live_oct(int up)                   /* OCT- / OCT+ on the page: the register */
{
    const int o = lv.oct + (up ? 1 : -1);
    if (o < -1 || o > 1)
        return;
    lv.oct = (int8_t)o;
    if (lv.root >= 0 && !lv.rest)
        lv_send();
}

static void live_snap(int32_t e)               /* ALGORITHM on the page: Snap */
{
    int v = (int)lv.snap + (int)e * 5;
    lv.snap = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    scopy(ui.glab[0], lv.snap < DR_SNAP_LEGATO ? "SNAP: WASH" : lv.snap < DR_SNAP_PAD ? "SNAP: LEGATO" : "SNAP: PAD");
    itoa_u(lv.snap, ui.glab[1]);
    ui.glab_until = plat_ms() + 1500u;
    ui.act_t = plat_ms();
}

static void live_run(void)                     /* each frame: Dark-Bright's octave for the upper tones */
{
    const int d = proj.par[P_DARK];
    const int r = d >= 50 ? 1 : d <= -50 ? -1 : (lv.reg > 0 && d > 40) || (lv.reg < 0 && d < -40) ? lv.reg : 0;
    if (r != lv.reg) {
        lv.reg = (int8_t)r;
        if (lv.root >= 0 && !lv.rest)
            lv_send();
    }
}

static int live_led(int k)                     /* a key's light on the page, 0..255; -1 not the page's */
{
    int m;
    if (lv.prog)                               /* the progression: next and previous, taking turns at full */
        return k > PG_PREV ? 0 : (int)(plat_ms() / 500u & 1u) == k ? 255 : 96;
    if (k < LV_NROOT)
        return k == lv.root ? 255 : k == lv.bass ? LED_DIM : 0;
    for (m = 0; m < LM_N; m++)
        if (k == LV_MOD_K[m])
            return lv.mods >> m & 1u ? 255 : 0;
    if (k == LV_BASS_K)
        return lv.bass >= 0 ? 255 : lv.bass_arm ? (blink(400u) ? 255 : 0) : 0;
    return 0;
}

/* the picture: the chord's name large, the four notes under it (dimmed as Thickness takes them away), the
 * modifiers armed for the next chord, the bass, the octave and Snap */
static void lk_pic_live(void)
{
    char t[24];
    int i;
    const float th = (float)proj.par[P_THICK] * 0.01f;
    if (lv.root >= 0)
        lv_name(lv.root, lv.smods, lv.bass, t);
    else
        scopy(t, "no chord");
    text_c(120, 14, lv.root >= 0 ? &FONT_L : &LK_F_SOUND, t, lv.root >= 0 && !lv.rest ? LK_WHITE : LK_LABEL);
    if (lv.root >= 0) {
        static const char *const ROLE[4] = {"bass", "5th", "3rd", "top"};
        for (i = 0; i < 4; i++) {
            /* Thickness's fades (drone.c LIVE_MV): the top 0 .. -35, the 3rd -35 .. -70, the 5th -70 .. -100 */
            const float lo = i == 3 ? 0.0f : i == 2 ? 0.35f : 0.70f, hi = i == 3 ? 0.35f : i == 2 ? 0.70f : 1.0f;
            const float left = i == 0 || th >= -lo ? 1.0f : 1.0f - fm_clampf((-th - lo) / (hi - lo), 0.0f, 1.0f);
            const int32_t cx = 30 + 60 * i;
            const int n = lv.note[i];
            char nn[8];
            scopy(nn, LV_PC[n % 12]);
            itoa_u((uint32_t)(n / 12 - 1), lv_end(nn));
            text_c(cx, 56, &FONT_T22, nn, lk_mix(0x5A566Bu, 0xFFFFFFu, left));
            text_c(cx, 80, &LK_F_SMALL, ROLE[i], LK_LABEL);
        }
    }
    {   /* the line under: what the next chord takes, the bass, the octave, Snap */
        int32_t x = 10;
        const int32_t y = LK_PIC_H - (int32_t)LK_F_TAG.h - 4;
        if (lv.prog) {                         /* the progression's steps, the one sounding in gold */
            for (i = 0; i < PG_N; i++) {
                char c[12];
                lv_name(PG_DEMO[i].root, PG_DEMO[i].mods, -1, c);
                x = cv_text(x, y, &LK_F_TAG, c, i == lv.step ? LK_GOLD : LK_LABEL);
                x = cv_text(x, y, &LK_F_TAG, " ", LK_LABEL);
            }
        } else if (lv.bass_arm)
            x = cv_text(x, y, &LK_F_TAG, "bass: tap a root", LK_GOLD);
        else if (lv.mods) {
            x = cv_text(x, y, &LK_F_TAG, "next ", LK_LABEL);
            for (i = 0; i < LM_N; i++)
                if (lv.mods >> i & 1u) {
                    x = cv_text(x, y, &LK_F_TAG, LV_MOD_NAME[i], LK_GOLD);
                    x = cv_text(x, y, &LK_F_TAG, " ", LK_LABEL);
                }
        }
        t[0] = 0;
        if (lv.prog)
            scat(t, lv.hold ? "hold  " : "rest  ");
        if (lv.oct)
            scat(t, lv.oct > 0 ? "oct +1  " : "oct -1  ");
        scat(t, "snap ");
        itoa_u(lv.snap, lv_end(t));
        if (230 - text_w(&LK_F_TAG, t) < x + 6)   /* no room: the octave alone (Snap shows as it turns) */
            scopy(t, lv.oct > 0 ? "oct +1" : lv.oct < 0 ? "oct -1" : "");
        cv_text(230 - text_w(&LK_F_TAG, t), y, &LK_F_TAG, t, LK_LABEL);
    }
}
