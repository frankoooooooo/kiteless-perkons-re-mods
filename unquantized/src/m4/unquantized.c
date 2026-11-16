/* unquantized: the trigger keys record where they were played, not on the nearest step.
 *
 * On (config row 3 step 13), while playing with REC on: a trigger key writes the step
 * playing now and its delay (the % of a step the step plays late, the value shuffle sets)
 * from how far into the step it was pressed. The delay is in the track's own clock ticks
 * (24 per step at every rate), so the timing holds at other tempos and rates. The hit has
 * already sounded and its tick has passed, so the step doesn't play again this time round.
 * Off: stock (the nearest step, its delay left as it was). */
#include <stdint.h>
#include "pk_host.h"
#include "fw_m4_seq.h"

PK_AXI uint8_t pk_cfg_unquantized;          /* system/mods.json "unquantized" */
PK_AXI static uint8_t played;               /* the trigger key being handled: row + 1, 0 none */

/* the global key handler (chained): which trigger key key_event is handling */
int PK_NEXT(pk_uq_key_event)(const uint8_t *evt);
int pk_uq_key_event(const uint8_t *evt)
{
    int trig = evt[0] - FW_KEY_TRIG(0);
    played = evt[1] == FW_EV_DOWN && trig >= 0 && trig < 4 ? (uint8_t)(trig + 1) : 0;
    int r = PK_NEXT(pk_uq_key_event)(evt);
    played = 0;
    return r;
}

/* key_event, right after: stock writes the nearest step when this returns 1 (playing) and
 * REC is on. Recording unquantized, write the step here and return 0: stock then stops. */
int PK_NEXT(pk_uq_playing)(void);
int pk_uq_playing(void)
{
    int state = PK_NEXT(pk_uq_playing)();
    if (state != 1 || !pk_cfg_unquantized || !FW_U8(SYM_rec_flags + FW_REC_ON) || !played)
        return state;
    uint8_t *vs = fw_voice_state(played - 1);
    uint32_t per = FW_U16(FW_U32(vs + FW_VS_CLOCK) + FW_CLK_STEP_TICKS);
    uint32_t tick = FW_U16(vs + FW_VS_TICK);
    int step = (int16_t)FW_U16(vs + FW_VS_STEP);
    if (per == 0 || step < 0 || step >= FW_STEPS)
        return state;
    if (tick >= per)
        tick = per - 1;
    uint32_t delay = (100 * tick + per - 1) / per;       /* the player: delay * per / 100 = tick */
    if (delay > 99)
        delay = 99;
    uint8_t *rec = (uint8_t *)(uintptr_t)FW_U32(vs + FW_VS_BLOCK) + step * FW_STEP_SIZE;
    /* the delay first: a clock tick in between finds the step's time already passed */
    *(volatile uint16_t *)(rec + FW_STEP_DELAY) = (uint16_t)delay;
    rec[7] = 0x7f;
    rec[0] = 1;
    return 0;
}
