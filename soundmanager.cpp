#include "soundmanager.h"

#include <QFile>
#include <QDataStream>
#include <QUrl>

#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

const int    RATE        = 44100;     // samples per second (mono, 16 bit)
const double MASTER_VOL  = 0.7;       // overall loudness of every effect
const int    MIN_GAP_MS  = 30;        // ignore the same effect twice within this time

enum Wave { SQUARE, PULSE, TRI, SAW, NOISE };

// Musical notes (Hz)
const double G3 = 196.00, C4 = 261.63, E4 = 329.63, G4 = 392.00;
const double C5 = 523.25, D5 = 587.33, E5 = 659.25, G5 = 783.99, A5 = 880.00;
const double C6 = 1046.50, E6 = 1318.51, G6 = 1567.98;

double rnd() { return std::rand() / double(RAND_MAX); }

// A tiny mixer: add tones at a start time, then write everything as a WAV file.
struct Mix
{
    std::vector<float> buf;

    // start/dur in seconds, frequency slides from f0 to f1, decay = how fast it fades
    void add(double start, double dur, double f0, double f1,
             Wave w, double vol, double decay = 1.3)
    {
        const int s0 = int(start * RATE);
        const int n  = int(dur * RATE);
        if (n <= 0 || f0 <= 0.0 || f1 <= 0.0) return;
        if (int(buf.size()) < s0 + n) buf.resize(s0 + n, 0.0f);

        const int attack = int(0.002 * RATE);     // 2 ms fade-in: no clicks
        double phase = 0.0;
        double held = 0.0;                        // sample-and-hold value for noise

        for (int i = 0; i < n; ++i) {
            const double p = double(i) / n;
            const double f = f0 * std::pow(f1 / f0, p);
            phase += f / RATE;
            if (phase >= 1.0) {
                phase -= 1.0;
                if (w == NOISE) held = rnd() * 2.0 - 1.0;
            }

            double s = 0.0;
            switch (w) {
            case SQUARE: s = phase < 0.5  ? 1.0 : -1.0;          break;
            case PULSE:  s = phase < 0.25 ? 1.0 : -1.0;          break;
            case TRI:    s = 4.0 * std::fabs(phase - 0.5) - 1.0; break;
            case SAW:    s = 2.0 * phase - 1.0;                  break;
            case NOISE:  s = held;                               break;
            }

            double env = std::pow(1.0 - p, decay);
            if (i < attack) env *= double(i) / attack;
            buf[s0 + i] += float(s * env * vol);
        }
    }

    bool saveWav(const QString &path) const
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;

        const quint32 bytes = quint32(buf.size() * 2);
        QDataStream ds(&f);
        ds.setByteOrder(QDataStream::LittleEndian);

        f.write("RIFF", 4);  ds << quint32(36 + bytes);
        f.write("WAVE", 4);
        f.write("fmt ", 4);  ds << quint32(16) << quint16(1) << quint16(1)
                                << quint32(RATE) << quint32(RATE * 2)
                                << quint16(2) << quint16(16);
        f.write("data", 4);  ds << bytes;

        for (size_t i = 0; i < buf.size(); ++i) {
            double v = buf[i] * 0.9;
            if (v >  1.0) v =  1.0;
            if (v < -1.0) v = -1.0;
            ds << qint16(v * 32767.0);
        }
        return true;
    }
};

} // namespace

// ---------------------------------------------------------------------------

SoundManager::SoundManager(QObject *parent)
    : QObject(parent)
{
    clock.start();
    for (int i = 0; i < SFX_COUNT; ++i) {
        nextSlot[i] = 0;
        lastPlayed[i] = -1000;
        for (int k = 0; k < POOL; ++k) effects[i][k] = nullptr;
    }

    if (!dir.isValid()) return;           // no temp folder: the game stays silent

    for (int i = 0; i < SFX_COUNT; ++i)
        build(i);
}

void SoundManager::build(int sfx)
{
    Mix m;

    switch (sfx) {
    case SFX_PADDLE:
        m.add(0, 0.07, 260, 160, SQUARE, 0.55, 1.2);
        break;

    case SFX_WALL:
        m.add(0, 0.05, 420, 380, PULSE, 0.35, 1.5);
        break;

    case SFX_BRICK_HIT:
        m.add(0, 0.07, 330, 230, TRI, 0.7, 1.2);
        m.add(0, 0.03, 3000, 2000, NOISE, 0.25, 1.0);
        break;

    case SFX_BRICK_BREAK_1:
    case SFX_BRICK_BREAK_2:
    case SFX_BRICK_BREAK_3:
    case SFX_BRICK_BREAK_4:
    case SFX_BRICK_BREAK_5: {
        // higher combo multiplier = higher note (C D E G A)
        static const double notes[5] = { C5, D5, E5, G5, A5 };
        const double f = notes[sfx - SFX_BRICK_BREAK_1];
        m.add(0.00, 0.10, f, f * 1.5, SQUARE, 0.45, 1.3);
        m.add(0.00, 0.05, 4000, 2500, NOISE, 0.30, 1.5);
        m.add(0.04, 0.08, f * 2, f * 2, PULSE, 0.18, 1.5);
        break;
    }

    case SFX_SOLID:
        m.add(0, 0.06, 1100, 900, TRI, 0.6, 1.2);
        m.add(0, 0.04, 2200, 2000, PULSE, 0.25, 2.0);
        break;

    case SFX_POWERUP: {
        static const double arp[4] = { C5, E5, G5, C6 };
        for (int k = 0; k < 4; ++k)
            m.add(k * 0.055, 0.09, arp[k], arp[k], SQUARE, 0.40, 1.0);
        break;
    }

    case SFX_LASER:
        m.add(0, 0.14, 1500, 250, SAW, 0.40, 1.0);
        break;

    case SFX_LAUNCH:
        m.add(0, 0.10, 220, 720, SQUARE, 0.35, 0.8);
        break;

    case SFX_LIFE_LOST:
        m.add(0, 0.55, 440, 70, SAW, 0.50, 1.0);
        m.add(0, 0.35, 300, 40, NOISE, 0.30, 1.2);
        break;

    case SFX_EXTRA_LIFE: {
        static const double jingle[5] = { C5, E5, G5, C6, E6 };
        for (int k = 0; k < 5; ++k) {
            m.add(k * 0.075, 0.14, jingle[k], jingle[k], SQUARE, 0.40, 1.0);
            m.add(k * 0.075, 0.14, jingle[k], jingle[k], TRI,    0.30, 1.0);
        }
        break;
    }

    case SFX_LEVEL_CLEAR: {
        static const double run[4] = { C5, E5, G5, C6 };
        for (int k = 0; k < 4; ++k)
            m.add(k * 0.10, 0.12, run[k], run[k], SQUARE, 0.40, 1.0);
        m.add(0.40, 0.45, C6, C6, SQUARE, 0.30, 0.8);
        m.add(0.40, 0.45, E6, E6, SQUARE, 0.22, 0.8);
        m.add(0.40, 0.45, G5, G5, TRI,    0.35, 0.8);
        break;
    }

    case SFX_GAME_OVER: {
        static const double down[3] = { G4, E4, C4 };
        for (int k = 0; k < 3; ++k)
            m.add(k * 0.22, 0.22, down[k], down[k], TRI, 0.60, 0.7);
        m.add(0.66, 0.65, G3, G3 * 0.5, SAW, 0.45, 1.0);
        break;
    }

    case SFX_WIN: {
        static const double tune[7]  = { C5, E5, G5, C6, G5, C6, E6 };
        static const double start[7] = { 0.00, 0.11, 0.22, 0.33, 0.50, 0.61, 0.72 };
        for (int k = 0; k < 7; ++k) {
            m.add(start[k], 0.12, tune[k], tune[k], SQUARE, 0.40, 1.0);
            m.add(start[k], 0.12, tune[k] * 0.5, tune[k] * 0.5, TRI, 0.30, 1.0);
        }
        m.add(0.90, 0.80, C6, C6, SQUARE, 0.28, 0.7);
        m.add(0.90, 0.80, E6, E6, SQUARE, 0.22, 0.7);
        m.add(0.90, 0.80, G6, G6, SQUARE, 0.18, 0.7);
        m.add(0.90, 0.80, C5, C5, TRI,    0.40, 0.7);
        break;
    }

    case SFX_START: {
        static const double up[3] = { G4, C5, E5 };
        for (int k = 0; k < 3; ++k)
            m.add(k * 0.07, 0.10, up[k], up[k], SQUARE, 0.35, 1.0);
        break;
    }

    case SFX_PAUSE:
        m.add(0.00, 0.06, 520, 520, SQUARE, 0.35, 1.0);
        m.add(0.07, 0.08, 360, 360, SQUARE, 0.35, 1.0);
        break;

    case SFX_RESUME:
        m.add(0.00, 0.06, 360, 360, SQUARE, 0.35, 1.0);
        m.add(0.07, 0.08, 520, 520, SQUARE, 0.35, 1.0);
        break;

    case SFX_CURSE:
        m.add(0.00, 0.30, 620, 140, SAW,    0.45, 0.9);
        m.add(0.04, 0.26, 470, 110, SQUARE, 0.25, 1.0);
        break;

    case SFX_EXPLODE:
        m.add(0, 0.45, 1400, 70, NOISE,  0.65, 1.0);
        m.add(0, 0.30, 140,  40, SQUARE, 0.40, 1.2);
        break;

    case SFX_ZAP:
        m.add(0, 0.20, 7000, 2500, NOISE, 0.45, 1.0);
        m.add(0, 0.16, 2200, 300,  SAW,   0.35, 1.0);
        break;

    case SFX_BOSS_HIT:
        m.add(0, 0.14, 190, 80, SQUARE, 0.50, 1.0);
        m.add(0, 0.06, 2500, 1200, NOISE, 0.30, 1.5);
        break;

    case SFX_BOSS_SHOOT:
        m.add(0, 0.20, 520, 170, SAW, 0.35, 1.0);
        break;

    case SFX_FREEZE:
        m.add(0.00, 0.22, 1800, 2600, TRI,   0.45, 1.0);
        m.add(0.03, 0.20, 2600, 1700, PULSE, 0.20, 1.2);
        break;

    case SFX_TYPE:
        m.add(0, 0.025, 880, 880, PULSE, 0.14, 1.5);
        break;

    case SFX_STAR:
        m.add(0.00, 0.07, G5, G5, SQUARE, 0.35, 1.0);
        m.add(0.06, 0.20, C6, C6, SQUARE, 0.35, 0.9);
        m.add(0.06, 0.20, E6, E6, TRI,    0.30, 0.9);
        break;

    default:
        return;
    }

    const QString path = dir.filePath(QString("sfx_%1.wav").arg(sfx));
    if (!m.saveWav(path)) return;

    for (int k = 0; k < POOL; ++k) {
        QSoundEffect *e = new QSoundEffect(this);
        e->setSource(QUrl::fromLocalFile(path));
        e->setVolume(MASTER_VOL);
        effects[sfx][k] = e;
    }
}

void SoundManager::play(int sfx)
{
    if (muted || sfx < 0 || sfx >= SFX_COUNT) return;

    const qint64 now = clock.elapsed();
    if (now - lastPlayed[sfx] < MIN_GAP_MS) return;
    lastPlayed[sfx] = now;

    // use a free player of this effect, otherwise take over the oldest one
    QSoundEffect *e = nullptr;
    for (int k = 0; k < POOL; ++k) {
        if (effects[sfx][k] && !effects[sfx][k]->isPlaying()) {
            e = effects[sfx][k];
            break;
        }
    }
    if (!e) {
        e = effects[sfx][nextSlot[sfx]];
        nextSlot[sfx] = (nextSlot[sfx] + 1) % POOL;
        if (!e) return;
        e->stop();
    }
    e->play();
}

void SoundManager::setMuted(bool m)
{
    muted = m;
    if (muted) {
        for (int i = 0; i < SFX_COUNT; ++i)
            for (int k = 0; k < POOL; ++k)
                if (effects[i][k]) effects[i][k]->stop();
    }
}

void SoundManager::toggleMuted()
{
    setMuted(!muted);
}
