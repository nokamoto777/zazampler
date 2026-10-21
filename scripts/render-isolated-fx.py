#!/usr/bin/env python3
"""Compare five isolated FX cases through the processor and through FX alone.

Only six FX-enable flags in a temporary bank copy are changed. Original assets
are never rewritten. Reference filenames are PREFIX1.flac ... PREFIX5.flac.
Requires numpy and soundfile. No resampling, alignment or normalization.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile

CASES = [('none', None), ('drive', 119), ('phaser', 116),
         ('delay', 117), ('reverb', 118)]


def isolate(data, slot, flag):
    if len(data) < 200 or data[:4] != b'CcnK' or data[8:12] != b'FBCh' or data[16:20] != b'ZMPL':
        raise ValueError('Expected a Zampler chunk FXB')
    params, count = struct.unpack_from('<II', data, 160)
    if params != 337 or not 1 <= count <= 128 or len(data) != 200 + count * 12780:
        raise ValueError('Unsupported bank layout')
    if struct.unpack_from('>I', data, 4)[0] != len(data)-8 or not 1 <= slot <= count:
        raise ValueError('Bank size or slot mismatch')
    result = bytearray(data)
    base = 200 + (slot-1)*12780 + 512
    for index in range(116, 122):
        struct.pack_into('<f', result, base + index*4, float(index == flag))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('renderer', type=Path)
    parser.add_argument('bank', type=Path)
    parser.add_argument('samples', type=Path)
    parser.add_argument('slot', type=int)
    parser.add_argument('midi', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--seconds', type=float, default=16)
    parser.add_argument('--reference-prefix', required=True, help='Full path up to and including #')
    parser.add_argument('--override', action='append', default=[], help='Parameter id=value; repeatable')
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error('--seconds must be positive')
    args.output.mkdir(parents=True, exist_ok=True)
    source = args.bank.read_bytes()
    report = {'bank_sha256': hashlib.sha256(source).hexdigest(),
              'midi_sha256': hashlib.sha256(args.midi.read_bytes()).hexdigest(),
              'slot': args.slot, 'seconds': args.seconds, 'overrides': args.override,
              'cases': [], 'note': 'No alignment or gain normalization. FX-only renders use original dry audio.'}
    spec = importlib.util.spec_from_file_location('comparison', Path(__file__).with_name('compare-reference-audio.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix='zazampler-isolation-') as temp:
        for number, (name, flag) in enumerate(CASES, 1):
            bank = Path(temp) / f'{name}.fxb'
            bank.write_bytes(isolate(source, args.slot, flag))
            output = (args.output / f'{number}-{name}.wav').resolve()
            command = [str(args.renderer.resolve()), '--render-bank', str(bank),
                       str(args.samples.resolve()), str(args.slot), str(args.midi.resolve()),
                       str(output), str(args.seconds), *args.override]
            subprocess.run(command, check=True)
            reference = Path(f'{args.reference_prefix}{number}.flac')
            row = {'case': name, 'full_processor': module.compare(reference, output)}
            fx_output = (args.output / f'{number}-{name}-fx-only.wav').resolve()
            subprocess.run([str(args.renderer.resolve()), '--render-fx', str(args.bank.resolve()),
                            str(args.slot), str(Path(f'{args.reference_prefix}1.flac').resolve()),
                            str(fx_output), name, *args.override], check=True)
            row['fx_only'] = module.compare(reference, fx_output)
            report['cases'].append(row)
    (args.output / 'comparison.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
