#!/usr/bin/env python3
"""Compare aligned audio without normalizing gain or silently shifting time.
Requires numpy and soundfile. Usage: script reference.flac rendered.wav
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import soundfile as sf


def compare(reference, rendered):
    a, sr = sf.read(reference, always_2d=True)
    b, br = sf.read(rendered, always_2d=True)
    if sr != br or a.shape != b.shape:
        raise ValueError('Sample rate, channel count and frame count must match; no implicit crop/resample')
    if not np.isfinite(a).all() or not np.isfinite(b).all():
        raise ValueError('Non-finite audio')
    ar, rr = np.linalg.norm(a), np.linalg.norm(b)
    if ar == 0 or rr == 0:
        raise ValueError('Silent reference or render')
    return {
        'sample_rate': sr, 'frames': len(a), 'channels': a.shape[1],
        'reference_sha256': hashlib.sha256(Path(reference).read_bytes()).hexdigest(),
        'render_sha256': hashlib.sha256(Path(rendered).read_bytes()).hexdigest(),
        'rms_difference_db': float(20*np.log10(rr/ar)),
        'relative_waveform_error': float(np.linalg.norm(b-a)/ar),
        'waveform_correlation': float(np.sum(a*b)/(ar*rr)),
        'reference_peak': float(np.max(np.abs(a))),
        'render_peak': float(np.max(np.abs(b))),
        'reference_near_full_scale_samples': int(np.count_nonzero(np.abs(a)>=.9999)),
        'render_near_full_scale_samples': int(np.count_nonzero(np.abs(b)>=.9999)),
        'note': 'No alignment or gain normalization. Correlation/error are not perceptual similarity scores.'
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference')
    parser.add_argument('rendered')
    args = parser.parse_args()
    print(json.dumps(compare(args.reference, args.rendered), indent=2))
