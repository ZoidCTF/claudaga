"""Cuts the bonus-round music from its source track.

The source is not in the repo - it is a 5MB CC0 download, and keeping it would
be carrying four tracks nothing uses to preserve one. That has now cost two
re-downloads, so the recipe lives here instead of in prose:

    https://opengameart.org/content/nes-shooter-music-5-tracks-3-jingles
    "NES Shooter Music" by SketchyLogic, CC0. Take Venus.wav from WAV.zip.

    python tools/make_bonus_music.py Venus.wav assets/audio/bonus.wav

Venus runs 25.7 seconds at 44100 Hz. The game wants a cut a little longer than
the longest bonus round so that no round ever loops it, downsampled to the
22050 Hz mono everything else here uses.

The cut lands at the quietest moment inside a window rather than at a fixed
time, so it falls between phrases instead of through one, and a short fade
covers what is left.
"""

import math
import struct
import sys
import wave

TARGET     = 22.0    # seconds; a little past the longest round
WINDOW     = 1.75    # search this far either side for somewhere quiet to cut
PROBE      = 0.05    # width of the quiet the search is looking for
FADE       = 0.25
OUT_RATE   = 22050


def read_wav(path):
    w = wave.open(path, 'rb')
    if w.getsampwidth() != 2:
        sys.exit('%s: expected 16-bit samples' % path)
    n, rate, ch = w.getnframes(), w.getframerate(), w.getnchannels()
    raw = w.readframes(n)
    w.close()
    d = struct.unpack('<%dh' % (n * ch), raw)
    if ch > 1:                      # fold to mono
        d = [sum(d[i:i + ch]) // ch for i in range(0, len(d), ch)]
    return list(d), rate


def quietest_cut(d, rate):
    """The lowest-energy probe window whose centre is within WINDOW of TARGET."""
    probe = int(PROBE * rate)
    lo    = int((TARGET - WINDOW) * rate)
    hi    = min(int((TARGET + WINDOW) * rate), len(d) - probe)
    if hi <= lo:
        return min(int(TARGET * rate), len(d))

    # A running sum of squares, so the search is one pass rather than one per
    # candidate.
    energy = sum(float(x) * x for x in d[lo:lo + probe])
    best, best_at = energy, lo
    for i in range(lo, hi):
        energy += float(d[i + probe]) ** 2 - float(d[i]) ** 2
        if energy < best:
            best, best_at = energy, i
    return best_at + probe // 2


def lowpass_halve(d, rate):
    """Decimate 2:1 behind a windowed-sinc filter, so nothing folds back."""
    taps, fc = 31, 0.227          # 10 kHz at 44100, comfortably under the new Nyquist
    mid = taps // 2
    h = []
    for i in range(taps):
        k = i - mid
        s = 2 * fc if k == 0 else math.sin(2 * math.pi * fc * k) / (math.pi * k)
        h.append(s * (0.54 - 0.46 * math.cos(2 * math.pi * i / (taps - 1))))
    gain = sum(h)
    h = [x / gain for x in h]

    out = []
    for i in range(0, len(d) - taps, 2):
        acc = 0.0
        for j in range(taps):
            acc += d[i + j] * h[j]
        out.append(acc)
    return out, rate // 2


def main():
    if len(sys.argv) != 3:
        sys.exit('usage: make_bonus_music.py <source.wav> <out.wav>')
    src, dst = sys.argv[1], sys.argv[2]

    d, rate = read_wav(src)
    print('source      %.2fs at %d Hz' % (len(d) / rate, rate))

    cut = quietest_cut(d, rate)
    print('cut at      %.2fs' % (cut / rate))
    d = d[:cut]

    fade = int(FADE * rate)
    for i in range(fade):
        d[len(d) - fade + i] *= (fade - i) / fade

    if rate == 2 * OUT_RATE:
        d, rate = lowpass_halve(d, rate)
    elif rate != OUT_RATE:
        sys.exit('%s: rate %d is neither %d nor twice it' % (src, rate, OUT_RATE))

    peak = max(abs(x) for x in d)
    print('out         %.2fs at %d Hz, peak %.3f' % (len(d) / rate, rate, peak / 32768))

    w = wave.open(dst, 'wb')
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(struct.pack('<%dh' % len(d),
                              *[max(-32768, min(32767, int(round(x)))) for x in d]))
    w.close()
    print('wrote       %s' % dst)


if __name__ == '__main__':
    main()
