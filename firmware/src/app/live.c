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
 *   OCT- / OCT+    the chords an octave down / up (-1 .. +1), the one lit where they are
 *   KNOB 1-4       on the PLAY page Dark-Bright, Thickness, Motion, Space (the live macros, a sound's)
 *   SELECT         the knobs' pages: PLAY, CHORDS, ARP, LEAD, MIX (below)
 *   PRESETS        the sound: P1 DETTIFOSS, P2 WIDE, P3 FELT, each with its pad, gliding there; the knobs stay
 *   ARP            a tap: the arp on / off (lit while on): the chord's tones in the octave over the bass, one at
 *                  a time, while a chord sounds. Held, the knobs: Tempo (a MIDI clock wins), Division, Pattern,
 *                  Length (the ARP page's, from any page)
 *   ALGORITHM      Motion speed: the LFO rate (Motion, KNOB 3, is how much)
 *   CHORDS' KNOB 1 Snap: how a change lands: a wash (a slow glide) .. legato (a short one) .. a pad's attack
 *
 * A chord is a bass and three tones over it: the third (or the sus), the fifth, the extension (or the root). Where they
 * sound is the voicing's (CHORDS' Height, the top note, and Spread: lv_voice); drone I's main note is the bass, drone
 * II's main the tone kept first, its partner the one needed least, drone I's partner the other, so Thickness, turned
 * down, takes the least needed away first.
 *
 * THE PROGRESSION (HOME on the Live page; HOME again: the chords): a progression's chords stepped through, a lead
 * on the keys above:
 *
 *   F3 / F#3       the next / the previous chord, both always lit, taking turns at full each 0.5 s. Two ways
 *                  (SEL + F3): REST: a tap steps; held, the chord is released, and letting go brings in the next
 *                  (air between).
 *                  HOLD: held, the chord plays; let go, it's released, and the next hold brings in the next.
 *                  While a chord rests the drones move to the next one's notes, so it comes in on them
 *   G5             BASS: the bass sounding held under the chords that follow, through rests too (lit), till tapped
 *                  again
 *   G3 .. F#5      the lead: one note at a time, the last key pressed; held notes glide; pressed harder,
 *                  brighter, with more vibrato. Its sound is the Preset's: Swell, Glass, Reed
 *   OCT- / OCT+    the lead an octave down / up (-1 .. +1); the chords keep the Live page's octave
 *   SELECT, the knobs, ALGORITHM, PRESETS   as on the Live page
 *
 * SEL + HOME, on either, opens Niagara Drift's Drones and Pitch pages (again: the other), till the sound has its own.
 *
 * For now the progression is the demo's: C, Am, Fmmaj7, Gm7, Cm7. Part of ui.c's unity build (after look.c) */

enum { LM_MIN, LM_SUS2, LM_SUS4, LM_7, LM_MAJ7, LM_ADD9, LM_N };
static const char *const LV_MOD_NAME[LM_N] = {"m", "sus2", "sus4", "7", "maj7", "add9"};
static const uint8_t LV_MOD_K[LM_N] = {13, 15, 17, 20, 22, 25};   /* F#4 G#4 A#4 C#5 D#5 F#5 */
static const char *const LV_PC[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
#define LV_BASS_K 26                           /* G5 */
#define LV_NROOT 12                            /* F3 .. E4, keys 0 .. 11 */
#define LV_LOW 41                              /* F3's root: its bass F2 (MIDI) */
#define PG_NEXT 0                              /* F3 */
#define PG_PREV 1                              /* F#3 */
#define PG_LEAD0 2                             /* G3 .. F#5: the lead (keys 2 .. 25) */
#define PG_NLEAD 24
typedef struct { int8_t root; uint8_t mods; } pg_chord_t;
static const pg_chord_t PG_DEMO[] = {{7, 0}, {4, 1u << LM_MIN}, {0, 1u << LM_MIN | 1u << LM_MAJ7}, {2, 1u << LM_MIN | 1u << LM_7},
                                     {7, 1u << LM_MIN | 1u << LM_7}};
#define PG_N ((int)(sizeof PG_DEMO / sizeof PG_DEMO[0]))

static struct {
    int8_t root;                               /* the chord sounding: its root key (0 .. 11), or -1 */
    uint8_t mods;                              /* the modifiers armed (bit LM_*) */
    uint8_t smods;                             /* ... the chord sounding's */
    int8_t bass;                               /* the bass set apart: its root key, or -1 (the chord's root) */
    uint8_t bass_arm;                          /* Bass tapped: the next root key sets the bass */
    int8_t oct;                                /* the register, octaves */
    int8_t top;                                /* the top note sounding (MIDI), 0 none yet: Height's next step */
    uint8_t snap;
    int16_t note[4];                           /* the chord's notes as sent, MIDI: bass, fifth, third, extension */
    uint8_t prog;                              /* the progression's page */
    uint8_t hold;                              /* ... its way: 0 REST (held, released), 1 HOLD (held, played) */
    int8_t step;                               /* the step sounding (or last sounded), -1 none yet */
    int8_t to;                                 /* the step a held key goes to, -1 none */
    uint8_t rest;                              /* the chord released */
    int8_t pbass;                              /* ... a bass held under it (a root key), -1 none */
    int16_t pnote;                             /* ... its note, MIDI, at no octave shift: it stays put */
    uint8_t mode;                              /* the chords' keys: LV_P, LV_Q, LV_R (SEL) */
    int8_t height;                             /* HEIGHT: the top note, MIDI, the octave's shift aside (CHORDS' KNOB 2) */
    int8_t spread;                             /* SPREAD: how far down from it the other two reach, semitones */
    uint16_t shape;                            /* Q: the right hand's shape, its tones over C (bit 0 C .. 11 B); 0
                                                * none yet (a major triad) */
    uint32_t rkeys;                            /* R: the keys of the chord (latched) */
    uint32_t rtap;                             /* ... those pressed again while others were held: a tap takes them out */
    uint8_t lh;                                /* Q: the left hand plays 0 the root and bass, 1 the bass (the root
                                                * held), 2 the root (the bass held) */
    uint8_t croot;                             /* Q and R: the chord's root, a pitch class (C 0), for its name */
    char cname[16];                            /* ... its name */
    int8_t lead[PG_NLEAD];                     /* the progression: the lead's keys held, the last pressed last */
    uint8_t nlead;
    int8_t lnote;                              /* ... the note sounding or last sounded, MIDI */
    int8_t loct;                               /* ... its octave (OCT on the progression), -1 .. 1 */
    uint8_t preset;                            /* the PRESET encoder's last Preset (0 .. LV_NPRESET - 1) */
    uint8_t lpress;                            /* the lead's key's pressure, as last sent */
    uint8_t arp, abpm, adiv, apat, alen;       /* the arp: on; its tempo, division, pattern, length (the engine's too) */
    uint8_t page;                              /* the knobs' page (SELECT): KP_PLAY .. KP_MIX */
    uint8_t lsnd, lpk;                         /* the lead's Sound (0 the Preset's, 1.. a kind + 1); the Preset's kind */
    uint8_t lglide, lvib, lpr;                 /* ... its Glide (10 ms), Vibrato, Press (%) */
    int8_t mix[4];                             /* the Mix page: the drones', pad's, lead's, arp's levels (dB, -31 off) */
} lv = {-1, 0, 0, -1, 0, 0, 0, 50, {0, 0, 0, 0}, 0, 0, -1, -1, 0, -1, 0, 0, 60, 12, 0, 0, 0, 0, 0, "", {0}, 0, 0, 0,
        0, 0, 0, 90, 1, 0, 40, 0, 0, 0, 7, 100, 100, {0, 0, 0, 0}};

/* THE THREE WAYS TO PLAY CHORDS (the chords' Keys, CHORDS' KNOB 4; a SEL tap on the Live page goes round them):
 *   P  roots and modifiers, as above
 *   Q  root and shape: the left hand (F3 .. E4) the root, as P's; the right hand (F4 .. F#5) a chord's shape, its
 *      keys read as over C (C E G a major chord, C Eb G Bb a minor seventh, whatever the root). Keys held together
 *      make one shape, latched; the next touch after letting go starts a new one. The left hand plays the root and
 *      the bass; OCT- lit, the bass alone (the chord's root held); OCT+ lit, the root alone (the bass held): a
 *      three-way switch, the lit one tapped again back to both. (No octave: Height places the chord)
 *   R  the whole keyboard: the keys held together are the chord, latched as Q's shape; the lowest the bass (an octave
 *      down, no higher than E3), the rest its tones. One key alone: its fifth and octave over it. While a key is held,
 *      a tap of a key in the chord takes it out (one held on while another is let go stays: the hand moved on)
 * In Q and R the tones aren't played where the keys are: up to three of them (the third and seventh kept first, the
 * root and fifth dropped first) are placed by the voicing below, as P's are */
enum { LV_P, LV_Q, LV_R, LV_NMODE };
#define LV_ZQ 0x03FFF000u                      /* Q's right hand: F4 .. F#5, keys 12 .. 25 */
#define LV_ALL 0x07FFFFFFu                     /* every key, F3 .. G5 */
#define LV_H_LO 52                             /* Height: E3 .. C7 */
#define LV_H_HI 96
#define LV_H_DEF 72                            /* ... C5 */
#define LV_S_HI 36
static int live_qr(void) { return lv.mode != LV_P && !lv.prog; }
static int live_q(void) { return lv.mode == LV_Q && !lv.prog; }

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

/* Q and R: the chord's name from its pitch classes (bit 0 C), and its tones for the voicing */
static const char *const LV_IV[12] = {"1", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7"};
static const struct { uint16_t m; char s[6]; } LV_KIND[] = {
    {0x091, ""}, {0x089, "m"}, {0x085, "sus2"}, {0x0A1, "sus4"}, {0x491, "7"}, {0x891, "maj7"}, {0x489, "m7"},
    {0x4A1, "7sus4"}, {0x095, "add9"}, {0x08D, "madd9"}, {0x291, "6"}, {0x289, "m6"}, {0x495, "9"}, {0x895, "maj9"},
    {0x48D, "m9"}, {0x889, "mmaj7"}, {0x049, "dim"}, {0x111, "aug"}, {0x249, "dim7"}, {0x449, "m7b5"}, {0x081, "5"},
    {0x001, ""}};
static const uint8_t LV_KEEP[12] = {4, 3, 10, 11, 2, 9, 5, 6, 8, 1, 7, 0};   /* the tones kept first, over the root */
static uint16_t lv_rot(uint16_t m, int r)      /* pitch classes as over r (r's bit 0) */
{
    r = (r % 12 + 12) % 12;
    return (uint16_t)((m >> r | m << (12 - r)) & 0xFFFu);
}
static int lv_kind(uint16_t m)                 /* its kind over its root, the fifth left out allowed; -1 none */
{
    int i;
    for (i = 0; i < (int)(sizeof LV_KIND / sizeof LV_KIND[0]); i++)
        if (LV_KIND[i].m == m || (!(m & 0x080u) && LV_KIND[i].m == (m | 0x080u) && m != 0x001u))
            return i;
    return -1;
}
static void lv_qr_name(int r, int b, uint16_t pcs)   /* over root r, bass b (pitch classes); "C:2 b6" if no kind */
{
    int k = lv_kind(lv_rot(pcs, r) | 1u), i;
    if (k < 0 && r == b)                       /* the bass as the root names nothing: another root, over it */
        for (i = 0; i < 12 && k < 0; i++)
            if (i != b && pcs >> i & 1u && (k = lv_kind(lv_rot(pcs, i))) >= 0)
                r = i;
    lv.croot = (uint8_t)r;
    scopy(lv.cname, LV_PC[r]);
    if (k >= 0)
        scat(lv.cname, LV_KIND[k].s);
    else {
        const uint16_t m = lv_rot(pcs, r);
        scat(lv.cname, ":");
        for (i = 1; i < 12 && text_w(&LK_F_TAG, lv.cname) < 150; i++)
            if (m >> i & 1u) {
                if (lv_end(lv.cname)[-1] != ':')
                    scat(lv.cname, " ");
                scat(lv.cname, LV_IV[i]);
            }
    }
    if (b != r) {
        scat(lv.cname, "/");
        scat(lv.cname, LV_PC[b]);
    }
}
static int lv_near(int pc, int t)              /* pitch class pc's note nearest note t */
{
    const int n = t - ((t - pc) % 12 + 12) % 12;
    return t - n > 6 ? n + 12 : n;
}
/* THE VOICING (every way to play chords, and the progression): the chord's three upper tones (the third, the seventh,
 * the ninth kept first: LV_KEEP) placed under HEIGHT, the top note, and reaching down by SPREAD. The top is the tone
 * nearest Height (so a chord change lands its top nearest the last one's: the voices lead); the other two below it,
 * near Spread / 2 and Spread under it (0: close under it, the nearest below; 12 an octave from the top to the lowest;
 * 36 three octaves), each as near as its pitch class goes, never the top's note or each other's. All of them over the
 * bass. u[] by the tones' rank, not their height: u[0] the one kept first (drone II's main), u[1] drone I's partner,
 * u[2] drone II's partner (so Thickness, turned down, takes the least needed away first) */
static int lv_below(int pc, int t, int under)   /* pc's note nearest t, under note under */
{
    int v = lv_near(pc, t);
    while (v >= under)
        v -= 12;
    return v;
}
static int lv_tones(uint16_t m, int r, int *tone)   /* up to three of pitch classes m (over root r: bit 0 r), by rank */
{
    int n = 0, i;
    for (i = 0; i < 12 && n < 3; i++)
        if (m >> LV_KEEP[i] & 1u)
            tone[n++] = (r + LV_KEEP[i]) % 12;
    if (!n)
        tone[n++] = r;
    return n;
}
static int lv_dist(int a, int b) { return a > b ? a - b : b - a; }
static void lv_voice(int *tone, int n, int bass, int16_t *u)   /* tone[3]: n of them (fewer: doubled) */
{
    const int h = lv.height + 12 * lv.oct;
    int i, t = 0, best = 1 << 30, v[3], o0, o1, m, l, a0, a1, b0, b1;
    for (i = n; i < 3; i++)                    /* fewer than three: doubled (an octave apart, below) */
        tone[i] = tone[i - n];
    for (i = 0; i < n; i++) {                  /* the top: the tone nearest Height (the lower of two as near) */
        const int x = lv_near(tone[i], h), d = lv_dist(x, h) * 2 + (x > h);
        if (d < best) {
            best = d;
            t = i;
        }
    }
    v[t] = lv_near(tone[t], h);
    o0 = t == 0 ? 1 : 0;                       /* the other two */
    o1 = t == 2 ? 1 : 2;
    m = v[t] - (lv.spread + 1) / 2;            /* their places: the middle, the lowest */
    l = v[t] - lv.spread;
    a0 = lv_below(tone[o0], m, v[t]);          /* o0 the middle, o1 the lowest ... */
    a1 = lv_below(tone[o1], l, a0 + 1);
    b1 = lv_below(tone[o1], m, v[t]);          /* ... or the other way round */
    b0 = lv_below(tone[o0], l, b1 + 1);
    if (a1 == a0)
        a1 -= 12;
    if (b0 == b1)
        b0 -= 12;
    if (lv_dist(a0, m) + lv_dist(a1, l) <= lv_dist(b1, m) + lv_dist(b0, l)) {
        v[o0] = a0;
        v[o1] = a1;
    } else {
        v[o0] = b0;
        v[o1] = b1;
    }
    for (i = 0; i < 3; i++) {                  /* over the bass */
        while (v[i] <= bass)
            v[i] += 12;
        u[i] = (int16_t)v[i];
    }
    lv.top = (int8_t)(v[0] > v[1] ? (v[0] > v[2] ? v[0] : v[2]) : (v[1] > v[2] ? v[1] : v[2]));
}

static int lv_low(uint32_t m)                  /* the lowest key of m */
{
    int k = 0;
    while (k < 31 && !(m >> k & 1u))
        k++;
    return k;
}
static void lv_place(uint16_t m, int r, int b)   /* the chord's notes from its pitch classes (over r) and bass b */
{
    int tone[3], n = lv_tones(m, r, tone);
    int16_t u[3];
    lv_voice(tone, n, b, u);
    lv.note[0] = (int16_t)b;
    lv.note[1] = u[1];
    lv.note[2] = u[0];
    lv.note[3] = u[2];
    dr_chord(0, lv.note[0] * 100, (lv.note[1] - lv.note[0]) * 100, 1);
    dr_chord(1, lv.note[2] * 100, (lv.note[3] - lv.note[2]) * 100, 1);
}
static void lv_notes_qr(void)
{
    int b, r, k;
    uint16_t pcs = 0;
    if (lv.mode == LV_Q) {
        const int lo = LV_LOW + lv.root + 12 * lv.oct;
        r = (5 + lv.root) % 12;
        b = lv.bass >= 0 ? LV_LOW + lv.bass + 12 * lv.oct : lo;
        if (b > lo)
            b -= 12;
        pcs = lv_rot(lv.shape ? lv.shape : 0x091u, 12 - r);
        lv_qr_name(r, b % 12, (uint16_t)(pcs | 1u << r));
    } else {
        const int low = lv_low(lv.rkeys);
        b = LV_LOW + low + 12 * lv.oct;
        while (b > 52 + 12 * lv.oct)
            b -= 12;
        for (k = low + 1; k < 27; k++)
            if (lv.rkeys >> k & 1u)
                pcs |= (uint16_t)(1u << (5 + k) % 12);
        if (!pcs)                              /* one key: its fifth and octave */
            pcs = (uint16_t)(1u << b % 12 | 1u << (b + 7) % 12);
        r = b % 12;
        lv_qr_name(r, r, (uint16_t)(pcs | 1u << r));
    }
    lv_place(lv_rot(pcs, r), r, b);
}

static void lv_notes(void)                     /* the chord's notes to the drones (taken up by dr_chord_go / a rest) */
{
    if (live_qr()) {
        lv_notes_qr();
        return;
    }
    const int lo = LV_LOW + lv.root + 12 * lv.oct;
    int b = lv.bass >= 0 ? LV_LOW + lv.bass + 12 * lv.oct : lo;
    if (b > lo)                                /* a bass set apart goes under the chord */
        b -= 12;
    if (lv.prog && lv.pbass >= 0)              /* the progression's held bass: where it was */
        b = lv.pnote + 12 * lv.oct;
    lv_place((uint16_t)(1u << lv_third(lv.smods) | 1u << 7 | 1u << lv_ext(lv.smods) % 12), (5 + lv.root) % 12, b);
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
    lv.bass = lv.pbass;
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
    dr_chord_rest(lv.snap, lv.pbass >= 0);     /* (a held bass sounds on through it) */
    lv.rest = 1;
}
static void pg_lead(uint32_t keys, uint32_t ch)   /* the lead: one note, the last key pressed of those held */
{
    int k, i, j;
    for (k = PG_LEAD0; k < PG_LEAD0 + PG_NLEAD; k++) {
        if (!(ch >> k & 1u))
            continue;
        for (i = j = 0; i < lv.nlead; i++)     /* (out of the list, pressed or let go) */
            if (lv.lead[i] != k)
                lv.lead[j++] = lv.lead[i];
        lv.nlead = (uint8_t)j;
        if (keys >> k & 1u)
            lv.lead[lv.nlead++] = (int8_t)k;
    }
    if (lv.nlead) {
        lv.lnote = (int8_t)(53 + lv.lead[lv.nlead - 1] + 12 * lv.loct);
        dr_lead(lv.lnote * 100, 1);
    } else if (lv.lnote)
        dr_lead(lv.lnote * 100, 0);
}
static void pg_lead_off(void)                  /* the page left: the lead let go */
{
    lv.nlead = 0;
    if (lv.lnote)
        dr_lead(lv.lnote * 100, 0);
}
static void pg_keys(uint32_t keys, uint32_t ch)
{
    int k;
    if (ch & (((1u << PG_NLEAD) - 1u) << PG_LEAD0))
        pg_lead(keys, ch);
    if ((ch & keys) >> LV_BASS_K & 1u) {       /* G5: the bass sounding held under the next chords, or let go */
        if (lv.pbass >= 0) {
            const int was = lv.bass;
            lv.pbass = lv.bass = -1;
            if (lv.root >= 0 && !lv.rest && was >= 0 && was != lv.root)
                lv_send();                     /* (under a chord of another root: back on its own) */
        } else if (lv.root >= 0) {
            lv.pbass = lv.bass = lv.bass >= 0 ? lv.bass : lv.root;   /* (sounding already: nothing to send) */
            lv.pnote = (int16_t)(lv.note[0] - 12 * lv.oct);
        }
    }
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

/* THE KNOBS' PAGES (SELECT on the Live page and the progression; the keys and the chord on screen stay as they are):
 *   1 PLAY     Dark-Bright, Thickness, Motion, Space (the live macros)
 *   2 CHORDS   Snap, Height (the top note), Spread (how far down the others reach), the chords' Keys P / Q / R (on the
 *              progression its Way, REST / HOLD)
 *   3 ARP      Tempo, Division, Pattern, Length (as ARP held, from any page)
 *   4 LEAD     the lead's Sound (the Preset's, or Swell, Glass, Reed whatever the Preset), Glide, Vibrato, Press
 *              (how much the key's pressure opens the filter and deepens the vibrato)
 *   5 MIX      the drones', the pad's, the lead's and the arp's levels, each in dB over its own (0 as the sound has
 *              it), down to off
 * SELECT stops at either end (a quick turn left: PLAY); HOME on another page back to PLAY. The page is the Live
 * page's and the progression's both */
enum { KP_PLAY, KP_CHORDS, KP_ARP, KP_LEAD, KP_MIX, KP_N };
static const char *const KP_NAME[KP_N] = {"PLAY", "CHORDS", "ARP", "LEAD", "MIX"};
static int live_page(void) { return lv.page; }
static const char *live_page_name(void) { return KP_NAME[lv.page]; }
static int live_npage(void) { return KP_N; }
static void live_page_turn(int32_t e)          /* SELECT on the page */
{
    int v = (int)lv.page + (e > 0 ? 1 : -1);
    v = v < 0 ? 0 : v >= KP_N ? KP_N - 1 : v;
    if (v == lv.page)
        return;
    lv.page = (uint8_t)v;
    ui.sig[0] = ui.sig[2] = 0;                 /* (the header and the cards anew) */
}

static void live_home(void)                    /* HOME: back to the Live page as it was, and on it the other */
{
    if (ui.view == V_LIVE && lv.page != KP_PLAY) {   /* another page: back to PLAY */
        lv.page = KP_PLAY;
        return;
    }
    lv.to = -1;
    pg_lead_off();
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
    if (lv.mode == LV_R) {                     /* R: the keys held together the chord */
        const uint32_t up = ch & ~keys & LV_ALL;
        if (up) {                              /* let go: a tap of a key already in the chord takes it out, unless
                                                * another key was let go while it was held (the hand moved on) */
            const uint32_t out = up & lv.rtap & lv.rkeys;
            lv.rtap &= ~up & ~(keys & LV_ALL);
            if (out && lv.root >= 0) {
                lv.rkeys &= ~out;
                if (lv.rkeys) {
                    lv.root = (int8_t)(lv_low(lv.rkeys) % 12);
                    lv_send();
                } else {
                    lv.root = -1;
                    dr_chord_go(0, lv.snap);
                }
            }
        }
        if (!(dn & LV_ALL))
            return;
        if (ui.btn >> B_SEL & 1u) {            /* SEL + a key: the chord let go */
            ui.btn_used |= 1u << B_SEL;
            if (lv.root >= 0) {
                lv.root = -1;
                dr_chord_go(0, lv.snap);
            }
            return;
        }
        if ((keys ^ ch) & LV_ALL & ~up) {      /* other keys held: added, or one in the chord marked (a tap takes
                                                * it out) */
            lv.rtap |= dn & LV_ALL & lv.rkeys;
            if (!(dn & LV_ALL & ~lv.rkeys))
                return;
            lv.rkeys |= dn & LV_ALL;
        } else {                               /* the first after letting go of all: a new chord */
            lv.rkeys = dn & LV_ALL;
            lv.rtap = 0;
        }
        lv.root = (int8_t)(lv_low(lv.rkeys) % 12);
        lv_send();
        return;
    }
    if (lv.mode == LV_Q && (dn & LV_ZQ)) {     /* Q: the right hand's shape */
        uint16_t m = 0;
        for (k = 12; k < 26; k++)
            if (dn >> k & 1u)
                m |= (uint16_t)(1u << (5 + k) % 12);
        lv.shape = ((keys ^ ch) & LV_ZQ) ? (uint16_t)(lv.shape | m) : m;
        if (lv.root >= 0)
            lv_send();
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
        if (lv.mode == LV_Q) {                 /* Q: the root and bass, the bass alone, or the root alone */
            const int to = lv.lh;
            if (to == 1) {                     /* the bass, the chord's root held (none yet: both) */
                lv.bass = (int8_t)k;
                if (lv.root < 0)
                    lv.root = (int8_t)k;
            } else if (to == 2) {              /* the root, the bass held */
                if (lv.bass < 0)
                    lv.bass = lv.root >= 0 ? lv.root : (int8_t)k;
                lv.root = (int8_t)k;
            } else {
                lv.root = (int8_t)k;
                lv.bass = -1;
            }
        } else if (lv.bass_arm) {              /* Bass armed: this key the bass */
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
    for (k = 0; k < LM_N && lv.mode == LV_P; k++) {
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
    if ((dn >> LV_BASS_K & 1u) && lv.mode != LV_Q) {
        if (lv.bass >= 0) {                    /* the lit Bass: let go, the bass the root again */
            lv.bass = -1;
            lv.bass_arm = 0;
            if (lv.root >= 0)
                lv_send();
        } else
            lv.bass_arm = (uint8_t)!lv.bass_arm;
    }
}

static int live_oct_lit(int up)                /* OCT's lights on the page: Q the left hand's (OCT- the bass alone, OCT+
                                                * the root alone); else the octave, the lead's on the progression (OCT-
                                                * lit an octave down, OCT+ up, both dark as the keys) */
{
    const int o = lv.prog ? lv.loct : lv.oct;
    if (live_q())
        return lv.lh == (up ? 2 : 1);
    return up ? o > 0 : o < 0;
}
static void live_oct(int up)                   /* OCT- / OCT+ on the page: the register; Q: the left hand's; the
                                                * progression: the lead's */
{
    if (lv.prog) {
        const int o = lv.loct + (up ? 1 : -1);
        if (o < -1 || o > 1)
            return;
        lv.loct = (int8_t)o;
        say("LEAD", o > 0 ? "AN OCTAVE UP" : o < 0 ? "AN OCTAVE DOWN" : "AS THE KEYS");
        if (lv.nlead)
            pg_lead(0, 0);                     /* (the note held, there now: a glide) */
        return;
    }
    if (live_q()) {                            /* the bass alone (OCT-), the root alone (OCT+), the lit one: both */
        lv.lh = (uint8_t)(lv.lh == (up ? 2 : 1) ? 0 : up ? 2 : 1);
        say("LEFT HAND", lv.lh == 1 ? "THE BASS ALONE" : lv.lh == 2 ? "THE ROOT ALONE" : "ROOT AND BASS");
        return;
    }
    const int o = lv.oct + (up ? 1 : -1);
    if (o < -1 || o > 1)
        return;
    lv.oct = (int8_t)o;
    if (lv.root >= 0 && !lv.rest)
        lv_send();
}

static void live_snap(int32_t e)               /* Snap (CHORDS' KNOB 1) */
{
    int v = (int)lv.snap + (int)e * 5;
    lv.snap = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    scopy(ui.glab[0], lv.snap < DR_SNAP_LEGATO ? "SNAP: WASH" : lv.snap < DR_SNAP_PAD ? "SNAP: LEGATO" : "SNAP: PAD");
    itoa_u(lv.snap, ui.glab[1]);
    ui.glab_until = plat_ms() + 1500u;
    ui.act_t = plat_ms();
}

#define LV_NPRESET 3                           /* the PRESET encoder's: P1 .. P3 */
static void live_preset(int32_t e)             /* PRESETS on the page: the next or previous Preset */
{
    int n = proj.bank == BANK_PRESETS && ui.scene_at >= 0 && ui.scene_at < LV_NPRESET ? ui.scene_at : lv.preset;
    char *q = ui.glab[1];
    n += e > 0 ? 1 : -1;
    n = n < 0 ? 0 : n >= LV_NPRESET ? LV_NPRESET - 1 : n;
    if (n != lv.preset || ui.scene_at != n)
        preset_go(n);
    lv.preset = (uint8_t)n;
    scopy(ui.glab[0], "PRESET");
    *q++ = 'P';
    *q++ = (char)('1' + n);
    *q++ = ' ';
    scopy(q, preset_name(n));
    ui.glab_until = plat_ms() + 1500u;
    ui.act_t = plat_ms();
}

static const char *const ARP_DIV_NAME[DR_ARP_NDIV] = {"1/4", "1/8", "1/8T", "1/16"};
static const char *const ARP_PAT_NAME[DR_ARP_NPAT] = {"UP", "UP 2 OCT", "DOWN", "DOWN 2 OCT", "UP-DOWN", "UP-DOWN 2 OCT",
                                                       "RANDOM", "RANDOM 2 OCT"};
static void live_arp(void)                     /* ARP tapped on the page: the arp on / off */
{
    lv.arp ^= 1u;
    dr_arp(lv.arp);
    scopy(ui.glab[0], "ARP");
    scopy(ui.glab[1], lv.arp ? "ON" : "OFF");
    ui.glab_until = plat_ms() + 1500u;
    ui.act_t = plat_ms();
}
static int live_arp_on(void) { return lv.arp; }
static void live_arp_knob(int i, int32_t e)    /* ARP held, a knob: Tempo, Division, Pattern, Length */
{
    char *q = ui.glab[1];
    int v;
    switch (i) {
    case 0:
        v = (int)lv.abpm + (int)e;
        lv.abpm = (uint8_t)(v < DR_ARP_BPM_MIN ? DR_ARP_BPM_MIN : v > DR_ARP_BPM_MAX ? DR_ARP_BPM_MAX : v);
        dr_arp_set(0, lv.abpm);
        scopy(ui.glab[0], "ARP TEMPO");
        if (dr_ext_spb > 0.0f)
            scopy(q, "MIDI CLOCK");
        else {
            itoa_u(lv.abpm, q);
            while (*q)
                q++;
            scopy(q, " BPM");
        }
        break;
    case 1:
        v = (int)lv.adiv + (e > 0 ? 1 : -1);
        lv.adiv = (uint8_t)(v < 0 ? 0 : v >= DR_ARP_NDIV ? DR_ARP_NDIV - 1 : v);
        dr_arp_set(1, lv.adiv);
        scopy(ui.glab[0], "ARP DIVISION");
        scopy(q, ARP_DIV_NAME[lv.adiv]);
        break;
    case 2:
        v = (int)lv.apat + (e > 0 ? 1 : -1);
        lv.apat = (uint8_t)(v < 0 ? 0 : v >= DR_ARP_NPAT ? DR_ARP_NPAT - 1 : v);
        dr_arp_set(2, lv.apat);
        scopy(ui.glab[0], "ARP PATTERN");
        scopy(q, ARP_PAT_NAME[lv.apat]);
        break;
    default:
        v = (int)lv.alen + (int)e * 2;
        lv.alen = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
        dr_arp_set(3, lv.alen);
        scopy(ui.glab[0], "ARP LENGTH");
        itoa_u(lv.alen, q);
        break;
    }
    ui.glab_until = plat_ms() + 1500u;
    ui.act_t = plat_ms();
}

static void live_mode_step(int dir)           /* the chords' Keys: P, Q, R in turn (dir 1) or back (-1); the chord kept */
{
    static const char *const SAY[LV_NMODE] = {"ROOTS + MODIFIERS", "ROOT + SHAPE", "WHOLE KEYBOARD"};
    char t[8];
    if (lv.prog)
        return;
    if (lv.mode == LV_R && lv.root >= 0)       /* the bass's key, as a root */
        lv.root = (int8_t)(lv_low(lv.rkeys) % 12);
    lv.mode = (uint8_t)((lv.mode + LV_NMODE + dir) % LV_NMODE);
    lv.bass_arm = 0;
    if (lv.mode == LV_R) {
        lv.bass = -1;
        lv.rkeys = lv.root >= 0 ? 1u << lv.root : 0u;
    }
    if (lv.root >= 0 && !lv.rest)
        lv_send();
    scopy(t, "LIVE ");
    t[5] = (char)('P' + lv.mode);
    t[6] = 0;
    say(t, SAY[lv.mode]);
}

static void live_sel(void) { live_mode_step(1); }   /* SEL (a tap) on the chords: the next Keys */

static void lv_way(int hold)                   /* the progression's way: 0 REST, 1 HOLD */
{
    lv.hold = (uint8_t)hold;
    say("PROGRESSION", lv.hold ? "HOLD TO PLAY" : "HOLD TO REST");
}
/* THE LEAD PAGE: the lead's Sound (the Preset's kind, or one chosen whatever the Preset), Glide, Vibrato, Press */
#define LV_MIX_OFF 31                          /* the Mix page: -31 dB is off */
#define LV_GLIDE_HI 100                        /* Glide: up to 1 s, in 10 ms */
static const char *const LEAD_NAME[DR_LEAD_KINDS] = {"Swell", "Glass", "Reed"};
static void live_lead_kind(int k)              /* a Preset's lead kind (-1: not a Preset's, the last one stays) */
{
    if (k >= 0 && k < DR_LEAD_KINDS)
        lv.lpk = (uint8_t)k;
    dr_lead_kind(lv.lsnd ? lv.lsnd - 1 : lv.lpk);
}
static int lv_step(int v, int32_t e, int d, int hi) /* v moved e detents of d, held in 0..hi */
{
    v += (int)e * d;
    return v < 0 ? 0 : v > hi ? hi : v;
}
static void live_lead_knob(int i, int32_t e)
{
    switch (i) {
    case 0:
        lv.lsnd = (uint8_t)lv_step(lv.lsnd, e > 0 ? 1 : -1, 1, DR_LEAD_KINDS);
        live_lead_kind(-1);
        break;
    case 1:
        lv.lglide = (uint8_t)lv_step(lv.lglide, e, 1, LV_GLIDE_HI);
        dr_lead_set(0, lv.lglide * 10);
        break;
    case 2:
        lv.lvib = (uint8_t)lv_step(lv.lvib, e, 10, 200);
        dr_lead_set(1, lv.lvib);
        break;
    default:
        lv.lpr = (uint8_t)lv_step(lv.lpr, e, 10, 200);
        dr_lead_set(2, lv.lpr);
        break;
    }
}

static int live_knob(int i, int32_t e)         /* a knob on a page but PLAY (PLAY's are the macros: 0) */
{
    int v;
    switch (lv.page) {
    case KP_PLAY: return 0;
    case KP_ARP:
        live_arp_knob(i, e);
        ui.glab_until = 0;                     /* (the cards say it) */
        return 1;
    case KP_CHORDS: break;
    case KP_LEAD:
        live_lead_knob(i, e);
        ui.glab_until = 0;
        return 1;
    default:                                   /* MIX: a layer's level, a dB a detent (under -30: off) */
        v = lv.mix[i] + (int)e;
        v = v < -LV_MIX_OFF ? -LV_MIX_OFF : v > 6 ? 6 : v;
        if (lv.mix[i] == -LV_MIX_OFF && e > 0)  /* (off, up: from -30) */
            v = -30 + (int)e - 1;
        lv.mix[i] = (int8_t)v;
        dr_mix(i, v);
        ui.glab_until = 0;
        return 1;
    }
    if (i == 0) {
        live_snap(e);
        ui.glab_until = 0;                     /* (its card says it) */
        return 1;
    }
    if (i == 3) {                              /* Keys (on the progression: the Way) */
        if (lv.prog)
            lv_way(e > 0);
        else
            live_mode_step(e > 0 ? 1 : -1);
        return 1;
    }
    if (i == 1) {                              /* Height: the top to the chord's next tone up or down (none
                                                * sounding: a semitone) */
        v = lv.root >= 0 && lv.top ? lv.top - 12 * lv.oct : lv.height;
        if (lv.root >= 0 && lv.top) {
            const uint16_t pcs = (uint16_t)(1u << lv.note[1] % 12 | 1u << lv.note[2] % 12 | 1u << lv.note[3] % 12);
            int k = e > 0 ? 1 : -1;
            v += k;
            while (!(pcs >> ((v + 12 * lv.oct) % 12 + 12) % 12 & 1u))
                v += k;
        } else
            v += (int)e;
        lv.height = (int8_t)(v < LV_H_LO ? LV_H_LO : v > LV_H_HI ? LV_H_HI : v);
    } else {
        v = lv.spread + (int)e;
        lv.spread = (int8_t)(v < 0 ? 0 : v > LV_S_HI ? LV_S_HI : v);
    }
    if (lv.root >= 0 && !lv.rest) {            /* a glide, never a pad's attack */
        lv_notes();
        dr_chord_go(1, lv.snap < DR_SNAP_LEGATO ? lv.snap : DR_SNAP_LEGATO);
    }
    return 1;
}
static uint32_t live_sig(void)                 /* what the knobs' cards show of the page */
{
    return (uint32_t)(uint8_t)lv.height | (uint32_t)(uint8_t)lv.spread << 8 | (uint32_t)lv.mode << 16 |
           (uint32_t)lv.prog << 18 | (uint32_t)lv.hold << 19 | (uint32_t)(uint8_t)lv.top << 7 | (uint32_t)lv.page << 20 |
           (uint32_t)(dr_ext_spb > 0.0f) << 23 | ((uint32_t)lv.snap << 24 ^
           ((uint32_t)lv.abpm | (uint32_t)lv.adiv << 8 | (uint32_t)lv.apat << 12 | (uint32_t)lv.alen << 16 |
            (uint32_t)lv.lsnd << 24 | (uint32_t)lv.lpk << 27) * 2654435761u ^
           ((uint32_t)(uint8_t)lv.mix[0] | (uint32_t)(uint8_t)lv.mix[1] << 8 | (uint32_t)(uint8_t)lv.mix[2] << 16 |
            (uint32_t)(uint8_t)lv.mix[3] << 24) * 2246822519u ^
           ((uint32_t)lv.lglide | (uint32_t)lv.lvib << 8 | (uint32_t)lv.lpr << 16) * 40503u);
}
static int live_mode(void) { return lv.prog ? -1 : lv.mode; }

/* a page's card but PLAY's (those are the macros, the parameters' own): its name, its value as large as fits, a word
 * under it; gray when it does nothing here (Height and Spread in P; the pages to come); a dot row for 0..100 */
static const char *const KP_CARD[KP_N][4] = {{"", "", "", ""}, {"Snap", "Height", "Spread", "Keys"},
                                             {"Tempo", "Division", "Pattern", "Length"},
                                             {"Sound", "Glide", "Vibrato", "Press"}, {"Drones", "Pad", "Lead", "Arp"}};
static const char *const ARP_PAT_CARD[DR_ARP_NPAT] = {"Up", "Up", "Down", "Down", "Up-dn", "Up-dn", "Rand", "Rand"};
static const char *lv_top_name(void)           /* the top note's place in the chord: "3rd on top" */
{
    static const char *const ON[12] = {"root on top", "b9 on top", "9th on top", "b3 on top", "3rd on top",
                                       "4th on top", "b5 on top", "5th on top", "#5 on top", "6th on top",
                                       "b7 on top", "7th on top"};
    const int r = live_qr() ? lv.croot : (5 + lv.root) % 12;
    return ON[((lv.top - r) % 12 + 12) % 12];
}
static void lk_live_card(int i, int32_t x, int32_t cx, int on)
{
    static const char *const KEYS_SUB[LV_NMODE] = {"roots", "shape", "keyboard"};
    const char *name = KP_CARD[lv.page][i], *sub = "";
    const uint16_t acc = LK_DRONE[LK_G];
    const felucca_font_t *fn, *fb;
    int dim = 0, frac = -1;
    char t[12];
    if (lv.page == KP_PLAY)
        return;
    if (lv.page == KP_CHORDS && i == 3 && lv.prog)
        name = "Way";
    t[0] = 0;
    switch (lv.page * 4 + i) {
    case KP_CHORDS * 4:
        itoa_u(lv.snap, t);
        sub = lv.snap < DR_SNAP_LEGATO ? "wash" : lv.snap < DR_SNAP_PAD ? "legato" : "pad";
        frac = lv.snap;
        break;
    case KP_CHORDS * 4 + 1:
        {                                      /* the top note sounding, and which of the chord's tones it is */
            const int top = lv.root >= 0 && lv.top ? lv.top : lv.height + 12 * lv.oct;
            scopy(t, LV_PC[top % 12]);
            itoa_u((uint32_t)(top / 12 - 1), lv_end(t));
            sub = lv.root >= 0 && lv.top ? lv_top_name() : "top note";
        }
        break;
    case KP_CHORDS * 4 + 2:
        itoa_u((uint32_t)lv.spread, t);
        sub = "semis";
        break;
    case KP_CHORDS * 4 + 3:
        if (lv.prog) {
            scopy(t, lv.hold ? "HOLD" : "REST");
            sub = lv.hold ? "held: plays" : "held: rests";
        } else {
            t[0] = (char)('P' + lv.mode);
            t[1] = 0;
            sub = KEYS_SUB[lv.mode];
        }
        break;
    case KP_ARP * 4:
        if (dr_ext_spb > 0.0f) {
            scopy(t, "MIDI");
            sub = "clock";
        } else {
            itoa_u(lv.abpm, t);
            sub = "BPM";
        }
        break;
    case KP_ARP * 4 + 1: scopy(t, ARP_DIV_NAME[lv.adiv]); break;
    case KP_ARP * 4 + 2:
        scopy(t, ARP_PAT_CARD[lv.apat]);
        sub = lv.apat & 1u ? "2 octaves" : "1 octave";
        break;
    case KP_ARP * 4 + 3:
        itoa_u(lv.alen, t);
        frac = lv.alen;
        break;
    case KP_LEAD * 4:
        scopy(t, LEAD_NAME[lv.lsnd ? lv.lsnd - 1 : lv.lpk]);
        sub = lv.lsnd ? "own" : "preset's";
        break;
    case KP_LEAD * 4 + 1:
        itoa_u(lv.lglide * 10u, t);
        sub = "ms";
        break;
    case KP_LEAD * 4 + 2:
    case KP_LEAD * 4 + 3:
        {
            const uint8_t v = i == 2 ? lv.lvib : lv.lpr;
            if (v)
                itoa_u(v, t);
            else
                scopy(t, "Off");
            sub = v ? "%" : "";
        }
        break;
    default:                                   /* MIX */
        {
            const int d = lv.mix[i];
            if (d <= -LV_MIX_OFF)
                scopy(t, "Off");
            else {
                if (d > 0)
                    scopy(t, "+");
                if (d < 0)
                    scopy(t, "-");
                itoa_u((uint32_t)(d < 0 ? -d : d), lv_end(t));
                sub = "dB";
            }
        }
        break;
    }
    if (on && !dim) {
        cv_round(x + 1, 4, 54, 81, 5, acc);
        cv_round(x + 2, 5, 52, 79, 4, LK_BG);
    }
    fn = lk_name_font(name);
    text_c(cx, 7 + (int32_t)(FONT_T20.h - fn->h) / 2, fn, name, on && !dim ? acc : LK_LABEL);
    fb = on ? &FONT_N1 : &FONT_N2;
    if (text_w(fb, t) > 50)
        fb = &FONT_N2;
    if (text_w(fb, t) > 54)
        fb = &FONT_U1;
    text_c(cx, 32, fb, t, dim ? LK_LABEL : LK_WHITE);
    if (*sub)
        text_c(cx, 32 + (int32_t)fb->h, &FONT_U2, sub, LK_LABEL);
    else if (frac >= 0)
        lk_dots(cx, 70, (float)frac / 100.0f, LK_G, 0);
}

/* THE PLAYER'S SETTINGS (the project's pl): taken up at the start, kept as they change (each frame: whatever moved
 * them), saved with the project */
static void live_restore(void)
{
    const int c = proj.pl.height;
    int k;
    lv.page = KP_PLAY;                         /* (the start: on PLAY) */
    lv.snap = proj.pl.snap > 100 ? 100 : proj.pl.snap;
    lv.mode = proj.pl.mode < LV_NMODE ? proj.pl.mode : LV_P;
    lv.hold = proj.pl.hold ? 1u : 0u;
    lv.height = (int8_t)(c < LV_H_LO ? LV_H_LO : c > LV_H_HI ? LV_H_HI : c);
    lv.spread = (int8_t)(proj.pl.spread < 0 ? 0 : proj.pl.spread > LV_S_HI ? LV_S_HI : proj.pl.spread);
    lv.oct = (int8_t)(proj.pl.oct < -1 ? -1 : proj.pl.oct > 1 ? 1 : proj.pl.oct);
    lv.loct = (int8_t)(proj.pl.loct < -1 ? -1 : proj.pl.loct > 1 ? 1 : proj.pl.loct);
    lv.abpm = proj.pl.abpm < DR_ARP_BPM_MIN ? DR_ARP_BPM_MIN : proj.pl.abpm > DR_ARP_BPM_MAX ? DR_ARP_BPM_MAX : proj.pl.abpm;
    lv.adiv = proj.pl.adiv < DR_ARP_NDIV ? proj.pl.adiv : 1u;
    lv.apat = proj.pl.apat < DR_ARP_NPAT ? proj.pl.apat : 0u;
    lv.alen = proj.pl.alen > 100 ? 100 : proj.pl.alen;
    dr_arp_set(0, lv.abpm);
    dr_arp_set(1, lv.adiv);
    dr_arp_set(2, lv.apat);
    dr_arp_set(3, lv.alen);
    lv.lsnd = proj.pl.lsnd <= DR_LEAD_KINDS ? proj.pl.lsnd : 0u;
    lv.lglide = proj.pl.lglide > LV_GLIDE_HI ? LV_GLIDE_HI : proj.pl.lglide;
    lv.lvib = proj.pl.lvib > 200 ? 200 : proj.pl.lvib;
    lv.lpr = proj.pl.lpress > 200 ? 200 : proj.pl.lpress;
    live_lead_kind(lv.lpk);
    dr_lead_set(0, lv.lglide * 10);
    dr_lead_set(1, lv.lvib);
    dr_lead_set(2, lv.lpr);
    for (k = 0; k < 4; k++) {
        const int d = proj.pl.mix[k];
        lv.mix[k] = (int8_t)(d < -LV_MIX_OFF ? -LV_MIX_OFF : d > 6 ? 6 : d);
        dr_mix(k, lv.mix[k]);
    }
}
static void live_keep(void)
{
    if (proj.pl.snap == lv.snap && proj.pl.mode == lv.mode && proj.pl.hold == lv.hold && proj.pl.height == lv.height &&
        proj.pl.spread == lv.spread && proj.pl.oct == lv.oct && proj.pl.loct == lv.loct && proj.pl.abpm == lv.abpm &&
        proj.pl.adiv == lv.adiv && proj.pl.apat == lv.apat && proj.pl.alen == lv.alen && proj.pl.lsnd == lv.lsnd &&
        proj.pl.lglide == lv.lglide && proj.pl.lvib == lv.lvib && proj.pl.lpress == lv.lpr &&
        !memcmp(proj.pl.mix, lv.mix, sizeof lv.mix))
        return;
    proj.pl.snap = lv.snap;
    proj.pl.mode = lv.mode;
    proj.pl.hold = lv.hold;
    proj.pl.height = lv.height;
    proj.pl.spread = lv.spread;
    proj.pl.oct = lv.oct;
    proj.pl.loct = lv.loct;
    proj.pl.abpm = lv.abpm;
    proj.pl.adiv = lv.adiv;
    proj.pl.apat = lv.apat;
    proj.pl.alen = lv.alen;
    proj.pl.lsnd = lv.lsnd;
    proj.pl.lglide = lv.lglide;
    proj.pl.lvib = lv.lvib;
    proj.pl.lpress = lv.lpr;
    memcpy(proj.pl.mix, lv.mix, sizeof lv.mix);
    mark_dirty();
}

static void live_run(void)                     /* each frame: the lead's pressure; the settings kept */
{
    if (lv.prog && lv.nlead) {                 /* the lead's key pressed harder: brighter, more vibrato */
        const int p = plat_pressure((uint32_t)lv.lead[lv.nlead - 1]);
        if (p >= 0 && (p - lv.lpress > 1 || lv.lpress - p > 1)) {
            lv.lpress = (uint8_t)p;
            dr_lead_press(p);
        }
    } else
        lv.lpress = 0;                         /* (let go, the engine's back at 0) */
    live_keep();
}

static int live_led(int k)                     /* a key's light on the page, 0..255; -1 not the page's */
{
    int m;
    if (lv.prog)                               /* the progression: next and previous, taking turns at full */
        return k == LV_BASS_K ? (lv.pbass >= 0 ? 255 : 0)
               : k > PG_PREV ? (lv.nlead && k == lv.lead[lv.nlead - 1] ? 255 : 0)
                             : (int)(plat_ms() / 500u & 1u) == k ? 255 : 96;
    if (lv.mode == LV_R)                       /* R: the chord's keys */
        return lv.root >= 0 && (lv.rkeys >> k & 1u) ? 255 : 0;
    if (lv.mode == LV_Q && (LV_ZQ >> k & 1u))  /* Q: the shape's keys (none yet: a major triad's, dim) */
        return lv.shape ? (lv.shape >> (5 + k) % 12 & 1u ? 255 : 0) : (0x091u >> (5 + k) % 12 & 1u ? LED_DIM : 0);
    if (k < LV_NROOT)
        return k == lv.root ? 255 : k == lv.bass ? LED_DIM : 0;
    for (m = 0; m < LM_N; m++)
        if (k == LV_MOD_K[m])
            return lv.mods >> m & 1u ? 255 : 0;
    if (k == LV_BASS_K && lv.mode == LV_Q)
        return 0;
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
    if (lv.root >= 0 && live_qr())
        scopy(t, lv.cname);
    else if (lv.root >= 0)
        lv_name(lv.root, lv.smods, lv.bass, t);
    else
        scopy(t, "no chord");
    text_c(120, 14, lv.root >= 0 && text_w(&FONT_L, t) <= 220 ? &FONT_L : &LK_F_SOUND, t,
           lv.root >= 0 && !lv.rest ? LK_WHITE : LK_LABEL);
    if (lv.root >= 0) {                        /* the bass, then the three over it low to high, each named by its
                                                * place in the chord */
        static const char *const ROLE[12] = {"root", "b9", "9th", "b3", "3rd", "4th", "b5", "5th", "#5", "6th", "b7", "7th"};
        const int r = live_qr() ? lv.croot : (5 + lv.root) % 12;
        uint8_t at[4] = {0, 1, 2, 3}, c;
        for (i = 1; i < 3; i++) {              /* the upper three by height */
            int j;
            for (j = 1; j < 4 - i; j++)
                if (lv.note[at[j]] > lv.note[at[j + 1]]) {
                    c = at[j];
                    at[j] = at[j + 1];
                    at[j + 1] = c;
                }
        }
        for (i = 0; i < 4; i++) {
            /* Thickness's fades (drone.c LIVE_MV), by the drones' voices: drone II's partner (u[2], the tone needed
             * least) 0 .. -35, its main (the one kept first) -35 .. -70, drone I's partner -70 .. -100 */
            const int v = at[i];
            const float lo = v == 3 ? 0.0f : v == 2 ? 0.35f : 0.70f, hi = v == 3 ? 0.35f : v == 2 ? 0.70f : 1.0f;
            const float left = v == 0 || th >= -lo ? 1.0f : 1.0f - fm_clampf((-th - lo) / (hi - lo), 0.0f, 1.0f);
            const int32_t cx = 30 + 60 * i;
            const int n = lv.note[v];
            char nn[8];
            scopy(nn, LV_PC[n % 12]);
            itoa_u((uint32_t)(n / 12 - 1), lv_end(nn));
            text_c(cx, 56, &FONT_T22, nn, lk_mix(0x5A566Bu, 0xFFFFFFu, left));
            text_c(cx, 80, &LK_F_SMALL, v ? ROLE[(n - r + 120) % 12] : "bass", LK_LABEL);
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
        } else if (live_qr() && !lv.bass_arm)
            x = cv_text(x, y, &LK_F_TAG, lv.mode == LV_R ? "R whole keyboard" : lv.lh == 1 ? "Q left: bass" : lv.lh == 2 ? "Q left: root" : "Q root + shape", LK_LABEL);
        else if (lv.bass_arm)
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
