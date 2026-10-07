#!/usr/bin/env python3
"""Generate original, distributable demo audio; no Zampler content is included."""
import math, pathlib, struct, wave
root = pathlib.Path(__file__).resolve().parents[1] / 'Demo'
root.mkdir(exist_ok=True)
rate = 48000
with wave.open(str(root / 'tone.wav'), 'wb') as w:
    w.setparams((1, 2, rate, rate, 'NONE', 'not compressed'))
    w.writeframes(b''.join(struct.pack('<h', round(12000*math.sin(2*math.pi*440*i/rate))) for i in range(rate)))
(root / 'Sine.sfz').write_text('<region> sample=tone.wav pitch_keycenter=69 loop_mode=loop_continuous loop_start=0 loop_end=47999 ampeg_attack=0.002 ampeg_release=0.05\n')
(root / 'Layers.sfz').write_text('''<global> ampeg_attack=0.002 ampeg_release=0.05
<group> lovel=1 hivel=63
<region> sample=tone.wav key=60 volume=-6
<group> lovel=64 hivel=127
<region> sample=tone.wav key=72 volume=-6
''')
(root / 'Offset.sfz').write_text('<region> sample=tone.wav key=69 offset=24000 end=35999 ampeg_release=0.01\n')
