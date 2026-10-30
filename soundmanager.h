#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <QObject>
#include <QSoundEffect>
#include <QTemporaryDir>
#include <QElapsedTimer>

// Retro sound effects for Brick Breaker.
// No audio files are needed: at start-up every effect is synthesized as a tiny
// 8-bit style WAV (square / triangle / saw / noise waves), saved into a
// temporary folder and loaded into QSoundEffect objects. Each effect has a
// small pool of players so quick, overlapping hits do not cut each other off.
class SoundManager : public QObject
{
    Q_OBJECT

public:
    enum Sfx {
        SFX_PADDLE = 0,
        SFX_WALL,
        SFX_BRICK_HIT,          // brick damaged but not destroyed
        SFX_BRICK_BREAK_1,      // destroyed brick: pitch rises with the combo
        SFX_BRICK_BREAK_2,
        SFX_BRICK_BREAK_3,
        SFX_BRICK_BREAK_4,
        SFX_BRICK_BREAK_5,
        SFX_SOLID,              // hit an unbreakable brick
        SFX_POWERUP,            // caught a power-up capsule
        SFX_LASER,
        SFX_LAUNCH,
        SFX_LIFE_LOST,
        SFX_EXTRA_LIFE,
        SFX_LEVEL_CLEAR,
        SFX_GAME_OVER,
        SFX_WIN,
        SFX_START,
        SFX_PAUSE,
        SFX_RESUME,
        SFX_CURSE,              // caught a curse (power-down) capsule
        SFX_EXPLODE,            // bomb brick / boss explosion
        SFX_ZAP,                // ZAP power-up lightning
        SFX_BOSS_HIT,
        SFX_BOSS_SHOOT,
        SFX_FREEZE,             // paddle hit by a shadow bolt
        SFX_TYPE,               // typewriter blip in the story scenes
        SFX_STAR,               // a star pops in on the level-clear screen
        SFX_COUNT
    };

    explicit SoundManager(QObject *parent = nullptr);

    void play(int sfx);
    void setMuted(bool m);
    bool isMuted() const { return muted; }
    void toggleMuted();

private:
    enum { POOL = 3 };

    QTemporaryDir dir;
    QSoundEffect *effects[SFX_COUNT][POOL];
    int nextSlot[SFX_COUNT];
    qint64 lastPlayed[SFX_COUNT];
    QElapsedTimer clock;
    bool muted = false;

    void build(int sfx);
};

#endif // SOUNDMANAGER_H
