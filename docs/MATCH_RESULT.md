# Match result

RUMBLING and Quick Rental now end when the player's robot dies, when the
player leaves, or when the player defeats the contest champion. RESULT owns
a snapshot of the final battle view, so the live world cannot move its backdrop
or restart combat under the receipt. Training and test drives return to their
own caller on death, without charging a rumbling repair bill.

The receipt includes starting money, prize, the rental deduction, right/left
arm and backpack repairs, body/leg repair, total repair and final NY. It shows
Retire, Win or Leave, shootdowns, and playable battle time (entrance/exit
cinematics excluded). The shared menu frame draws day, money, music credit and
the audio-driven visualizer; the hint scrolls through the header. The original
RESULT atlas supplies the frame, receipt, labels, Retire and buttons.

## Recovered rules

European `default.xbe`, researcher-assigned meanings:

- `0x091a61`: capture starting wallet, kills, time, rental status and repairs.
  Rental prize is shifted right once. A weapon incurs a repair charge if lost
  or if the player died; an empty slot costs nothing. Body and legs are charged
  only on death, using their combined prices.
- Repair = truncate(base-level price * 0.20 + current-level price * 0.01).
  The floats are at `0x19b584` and `0x199f78`. Both prices, the rounding and the
  rental division operate in stored tens of NY; display multiplies by ten.
- `0x096c45`: cap net losses at the player's wallet. Native settlement also
  respects the existing wallet ceiling.
- `0x094fee`, static table `0x249a60`: RESULT/receipt layout; `0x249bb0`:
  rental annotations (2271 RENTAL during counting, 2272 50% OFF afterward).
  Native uses 2272 throughout. `0x09547d`: LOSER music on retirement and WINNER
  on a surviving result.

`data/ui/match_result.json` owns repair/rental tuning, source sprite rectangles,
row positions, tally duration, hint and music. Native presentation uses a
simultaneous three-second count, skippable with A/START or B/Back. A subsequent
press activates the selected REPLAY or EXIT button. Settlement runs once
on entry, independent of the tally or button presses. The live world's final
kill is collected even if its next 30 Hz observation has not occurred yet.

REPLAY watches the recorded match and returns to this same receipt when stopped
or finished; see [REPLAY.md](REPLAY.md). EXIT follows scene 30 to scene 24:
an eligible post-match conversation plays first, then the existing next-day
sound/transition advances the day and opens the Garage (scene 13). Story events
with conditions the existing story engine cannot yet evaluate remain gated.
The receipt background is a frozen native view, and Win/Leave are text labels;
this is not an exact reproduction of the original result animation or verdict grading.

## Validation

Core tests cover the 200,000 + 90 - 8,820 = 191,270 NY rental example, survival
versus retirement repairs, empty slots, level prices, rounding, wallet limits,
and final-tick bounty collection without duplicate payment. A temporary
one-hit-player runtime exercised death through the actual battle and screen
runner; production armour and shield tuning are unchanged.

Runtime checks cover voluntary exit, retirement, repeated playback, and leaving
RESULT with the same final balance and exactly one settlement. Post-match routes
were checked both with an eligible conversation and directly through next day
to the Garage.
