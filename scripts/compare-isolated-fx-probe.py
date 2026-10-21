#!/usr/bin/env python3
"""053 Dwarves Home isolated-FX experiment (numpy, scipy, soundfile required).
Usage: script FX_PROBE REFERENCE_DIRECTORY OUTPUT_DIRECTORY
Uses measured patch settings explicitly, not the FXB importer or sampler.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfilt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    parser.add_argument('references', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    spec = importlib.util.spec_from_file_location('comparison', Path(__file__).with_name('compare-reference-audio.py'))
    comparison = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(comparison)
    refs = [args.references / f'zampler-rx-assult-053-#{i}.flac' for i in range(1, 6)]
    audio = []
    for path in refs:
        samples, rate = sf.read(path, always_2d=True)
        if rate != 44100 or samples.shape != (705600, 2) or not np.isfinite(samples).all():
            raise ValueError(f'Expected finite 16-second, 44.1 kHz stereo: {path}')
        audio.append(samples)
    dry = args.output / 'dry.f32'
    audio[0].astype(np.float32).tofile(dry)
    # Current BankImport -> FxSettings values for the supplied 053 patch.
    cases = {
        'none': (0, {}),
        'drive': (1, {'drive': 15, 'driveMix': 1}),
        'phaser': (2, {'phaserMix': 1, 'phaserRate': .05, 'phaserDepth': 0, 'phaserFeedback': 0}),
        'delay': (3, {'delayMix': .325, 'delayMs': 20 * 100**.405,
                      'delayFeedback': .95 * .325, 'delayPingPong': 1}),
        'reverb': (4, {'reverb': .515, 'room': .63, 'damping': 1, 'width': 1}),
    }
    result = {'method': 'Production Effects.cpp; original dry as input, no sampler/filter/voice path, no alignment or normalization.',
              'settings_source': 'Explicit current import values of supplied patch 053, rounded raw values; not a live importer test.',
              'block_frames': 128, 'input_sha256': hashlib.sha256(refs[0].read_bytes()).hexdigest(),
              'results': {}}
    for name, (index, settings) in cases.items():
        for squared in ([False, True] if name in ('delay', 'reverb') else [False]):
            label = name + ('-squared' if squared else '-linear')
            parameters = dict(settings, spaceSquaredMix=int(squared))
            raw = args.output / (label + '.f32')
            subprocess.run([str(args.probe.resolve()), str(dry), str(raw), '44100',
                            *[f'{key}={value}' for key, value in parameters.items()]], check=True)
            rendered = np.fromfile(raw, dtype=np.float32).reshape(-1, 2)
            if rendered.shape != audio[0].shape:
                raise ValueError('Probe output length mismatch')
            wav = args.output / (label + '.wav')
            sf.write(wav, rendered, 44100, subtype='FLOAT')
            result['results'][label] = {'settings': parameters, **comparison.compare(refs[index], wav)}
    # Diagnostic only: reverberant/delayed high-frequency content can bias this
    # estimate, so it is not a proof of the proprietary Mix curve.
    sos = butter(4, 10000, btype='highpass', fs=44100, output='sos')
    x = sosfilt(sos, audio[0][:352800], axis=0)
    result['dry_coefficient_estimate'] = {}
    for name, index, amount in [('delay', 3, .325), ('reverb', 4, .515)]:
        y = sosfilt(sos, audio[index][:352800], axis=0)
        result['dry_coefficient_estimate'][name] = {
            'original': float(np.sum(x*y)/np.sum(x*x)),
            'linear': 1-amount, 'squared': 1-amount**2,
            'method': 'First 8 s, 4th-order 10 kHz Butterworth high-pass, zero-intercept regression'}
    report = args.output / 'comparison.json'
    report.write_text(json.dumps(result, indent=2) + '\n')
    print(report)
    for name, metrics in result['results'].items():
        print(f"{name}: RMS delta {metrics['rms_difference_db']:+.3f} dB, "
              f"correlation {metrics['waveform_correlation']:.4f}, error {metrics['relative_waveform_error']:.4f}")


if __name__ == '__main__':
    main()
