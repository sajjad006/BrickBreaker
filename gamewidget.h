#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QImage>
#include <QColor>
#include <QString>

#include "soundmanager.h"
#include "leaderboard.h"
#include "story.h"

class QPainter;

// Pixel-art Brick Breaker.
// The whole game is drawn into a tiny 208 x 240 pixel image (26 x 30 tiles of
// 8 x 8 pixels) and then scaled up by a whole number, so every pixel stays
// sharp and blocky. Only plain rectangles and small bitmaps are used.
// All collections are fixed-size arrays (no std::vector / QVector / QList).
class GameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    void tick();

private:
    enum {
        ROWS = 8,
        COLS = 12,
        MAX_BALLS = 12,
        MAX_POWERUPS = 10,
        MAX_PARTICLES = 300,
        MAX_LASERS = 24,
        MAX_BOLTS = 12,         // Morvax's shadow bolts
        MAX_POPUPS = 16,        // floating "+30" score numbers
        MAX_ZAPS = 6,           // lightning strikes of the ZAP power-up
        MAX_SCORES = 10,        // size of the high-score table
        MENU_ITEMS = 6,
        TRAIL = 8               // length of the menu ball's trail
    };

    enum State { Menu, HowTo, Scores, EnterName, WorldMap, Scene, Ending,
                 Ready, Playing, Paused, LevelClear, GameOver, Won };
    enum MenuItem { MI_STORY = 0, MI_ARCADE, MI_SCORES, MI_HOWTO, MI_SOUND, MI_QUIT };
    enum PowerType {
        PU_WIDE = 0, PU_SLOW, PU_MULTI, PU_LASER, PU_STICKY,
        PU_FIRE, PU_BARRIER, PU_ZAP,                    // newer power-ups
        PD_TINY, PD_FAST, PD_REVERSE, PD_DARK,          // curses: dodge these
        PU_COUNT
    };

    struct Brick    { bool alive; bool solid; bool heart; bool bomb; bool ghost; int hp; int maxHp; };
    struct Ball     { bool active; bool stuck; double x, y, vx, vy; double stuckOffset; };
    struct PowerUp  { bool active; double x, y; int type; };
    struct Particle { bool active; double x, y, vx, vy, life, maxLife; QColor color; };
    struct Laser    { bool active; double x, y; };
    struct ScoreEntry { char name[4]; int score; int level; char date[9]; };
    struct Bolt     { bool active; double x, y, vx, vy; };
    struct Popup    { bool active; double x, y, life; int value; };
    struct Zap      { bool active; double x, y, life; };
    struct Boss {
        bool active, dying;
        double x, y, vx;            // top-left corner and horizontal speed
        int hp, maxHp, phase;       // phase 0..2: he gets faster and angrier
        double hitFlash, attackTimer, dyingTimer;
    };

    // ---- game data (fixed arrays) ----
    Brick    bricks[ROWS][COLS];
    Ball     balls[MAX_BALLS];
    PowerUp  powerups[MAX_POWERUPS];
    Particle particles[MAX_PARTICLES];
    Laser    lasers[MAX_LASERS];
    Bolt     bolts[MAX_BOLTS];
    Popup    popups[MAX_POPUPS];
    Zap      zaps[MAX_ZAPS];
    Boss     boss;
    ScoreEntry scores[MAX_SCORES];      // high-score table, best first
    int      scoreCount = 0;

    State state = Menu;
    int score = 0;
    int highScore = 0;
    int lives = 3;
    int level = 1;
    int combo = 0;
    int bricksLeft = 0;     // breakable bricks left (the heart brick is optional)
    int bricksBroken = 0;
    int lastBonus = 0;

    // menu / high-score screens
    int    menuSel = 0;             // highlighted menu item
    int    scoreHighlight = -1;     // table row to flash (the score just saved)
    bool   clearArmed = false;      // first DEL press done, waiting for the second
    int    nameIdx[3] = { 0, 0, 0 };  // initials being typed (index into the letter list)
    int    nameCursor = 0;
    double animTime = 0.0;          // seconds, drives all menu animation
    double confettiTimer = 0.0;
    double endLock = 0.0;           // brief input lock right after game over / win
    // little demo ball that bounces behind the menu
    double mbx = 70.0, mby = 150.0, mvx = 52.0, mvy = -68.0, demoPadX = 104.0;
    double trailX[TRAIL], trailY[TRAIL], trailTimer = 0.0;
    double flash[COLS];             // brick flash timers for the menu bricks
    int    howToPage = 0;

    // story mode (progress is saved with QSettings)
    bool   storyMode = false;
    int    storyUnlocked = 1;               // highest level opened by playing
    int    storyStars[Story::LEVELS + 1];   // best stars per level (index 1..LEVELS)
    int    storyBest[Story::LEVELS + 1];    // best score per level
    int    mapSel = 1;                      // selected node on the world map
    double mapCam = 0.0;                    // world-map scroll (map pixels)
    int    livesLost = 0;                   // in this attempt: decides the stars
    int    starsEarned = 0;
    int    starsShown = 0;                  // stars popped in on the clear screen so far
    bool   newBest = false;
    double clearTimer = 0.0;
    const Story::Page *scenePages = nullptr;
    int    sceneCount = 0;
    int    scenePage = 0;
    double sceneChars = 0.0;                // typewriter: characters shown so far
    bool   sceneIsOutro = false;

    double paddleX = 104.0; // center of the paddle (pixels)
    double paddleW = 32.0;

    double wideTimer = 0.0;
    double slowTimer = 0.0;
    double laserTimer = 0.0;
    double stickyTimer = 0.0;
    double fireTimer = 0.0;
    double tinyTimer = 0.0;         // curses
    double fastTimer = 0.0;
    double reverseTimer = 0.0;
    double darkTimer = 0.0;
    double freezeTimer = 0.0;       // paddle frozen by a shadow bolt
    int    barrier = 0;             // barrier charges: each one saves a ball once
    double laserCooldown = 0.0;
    double shakeTimer = 0.0;
    double flashTimer = 0.0;        // white screen flash (explosions)
    double messageTimer = 0.0;
    QString message;
    QColor  messageColor;

    bool leftHeld = false;
    bool rightHeld = false;
    bool fireHeld = false;

    QImage frame;           // the small pixel canvas
    QTimer timer;
    QElapsedTimer clock;
    SoundManager sound;     // synthesized sound effects (M = mute)
    Leaderboard scoreDb;    // SQLite file that stores the high-score table

    // ---- game flow ----
    void newGame();
    void loadLevel(int lvl);
    void startNextLevel();
    void levelComplete();
    void loseLife();
    void grantLife();
    void resetBallOnPaddle();
    void resetEffects();
    void primaryAction();
    void togglePause();
    void goMenu(int sel = -1);
    void activateMenu(int item);
    void finishRun();               // run is over: name entry if it made the table, else menu
    QString menuLabel(int item) const;
    int  menuItemAt(double lx, double ly) const;
    bool isMenuState() const;
    void handleMenuKey(QKeyEvent *e);
    void handleNameKey(QKeyEvent *e);
    void handleInfoKey(QKeyEvent *e);
    void updateMenuDemo(double dt);

    // ---- high scores (saved in a local SQLite database, see leaderboard.h) ----
    void loadScores();
    bool qualifies(int s) const;
    int  scoreRank(int s) const;
    int  insertScore(const char *name, int s, int lvl);
    void clearScores();
    void beginNameEntry();
    void confirmName();
    void showMessage(const QString &text, double seconds,
                     const QColor &color = QColor(255, 240, 150));

    // ---- story mode ----
    void loadStoryProgress();
    void saveStoryResult(int lvl, int stars, int s);
    int  unlockedCount() const;
    int  totalStars() const;
    void openMap(int sel, bool snap);
    void playSelected();
    void startStoryLevel(int lvl, bool withIntro);
    void continueStory();
    void beginScene(const Story::Page *pages, bool outro);
    void advanceScene();
    void skipScene();
    int  wrapScenePage(QString out[], int maxLines) const;
    int  scenePageLength() const;
    void beginEnding();
    int  mapNodeAt(double lx, double ly) const;
    void handleMapKey(QKeyEvent *e);
    void handleSceneKey(QKeyEvent *e);
    bool isBossLevel() const;

    // ---- simulation ----
    double currentSpeed() const;
    double basePaddleW() const;
    int    rollPowerType() const;
    bool   ghostSolid(int r, int c) const;
    void updateBoss(double dt);
    void updateBolts(double dt);
    void bossHit(int damage);
    void spawnBolt(double x, double y, double vx, double vy);
    void spawnPopup(double x, double y, int value);
    void spawnZap(double x, double y);
    void zapBricks();
    void explodeBrick(int r, int c);
    void updateBalls(double dt);
    void updatePowerUps(double dt);
    void updateLasers(double dt);
    void updateParticles(double dt);
    void launchBalls();
    void fireLasers();
    void spawnLaser(double x);
    void spawnBall(double x, double y, double vx, double vy);
    void spawnPowerUp(double x, double y, int type);
    void spawnParticles(double x, double y, const QColor &col, int n);
    void applyPowerUp(int type);
    void hitBrick(int r, int c, int damage);
    int  activeBallCount() const;

    // ---- helpers ----
    QColor brickColor(int r, int c) const;
    int    viewScale() const;
    int    viewOffsetX() const;
    int    viewOffsetY() const;
    double toLogicalX(double widgetX) const;
    double toLogicalY(double widgetY) const;

    // ---- drawing (all into the small pixel canvas) ----
    void renderFrame();
    void drawHud(QPainter &p);
    void drawParticles(QPainter &p);
    void renderMenuScreen(QPainter &p);
    void drawMenu(QPainter &p);
    void drawScoresScreen(QPainter &p);
    void drawHowToScreen(QPainter &p);
    void drawNameScreen(QPainter &p);
    void drawBricks(QPainter &p);
    void drawBoss(QPainter &p);
    void drawDarkness(QPainter &p);
    void drawStoryClear(QPainter &p);
    void drawMapScreen(QPainter &p);
    void drawSceneScreen(QPainter &p);
    void drawEndingScreen(QPainter &p);
};

#endif // GAMEWIDGET_H
