#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QImage>
#include <QColor>
#include <QString>

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
        MAX_LASERS = 24
    };

    enum State { Title, Ready, Playing, Paused, LevelClear, GameOver, Won };
    enum PowerType { PU_WIDE = 0, PU_SLOW, PU_MULTI, PU_LASER, PU_STICKY };

    struct Brick    { bool alive; bool solid; bool heart; int hp; int maxHp; };
    struct Ball     { bool active; bool stuck; double x, y, vx, vy; double stuckOffset; };
    struct PowerUp  { bool active; double x, y; int type; };
    struct Particle { bool active; double x, y, vx, vy, life, maxLife; QColor color; };
    struct Laser    { bool active; double x, y; };

    // ---- game data (fixed arrays) ----
    Brick    bricks[ROWS][COLS];
    Ball     balls[MAX_BALLS];
    PowerUp  powerups[MAX_POWERUPS];
    Particle particles[MAX_PARTICLES];
    Laser    lasers[MAX_LASERS];

    State state = Title;
    int score = 0;
    int highScore = 0;
    int lives = 3;
    int level = 1;
    int combo = 0;
    int bricksLeft = 0;     // breakable bricks left (the heart brick is optional)
    int bricksBroken = 0;
    int lastBonus = 0;

    double paddleX = 104.0; // center of the paddle (pixels)
    double paddleW = 32.0;

    double wideTimer = 0.0;
    double slowTimer = 0.0;
    double laserTimer = 0.0;
    double stickyTimer = 0.0;
    double laserCooldown = 0.0;
    double shakeTimer = 0.0;
    double messageTimer = 0.0;
    QString message;

    bool leftHeld = false;
    bool rightHeld = false;
    bool fireHeld = false;

    QImage frame;           // the small pixel canvas
    QTimer timer;
    QElapsedTimer clock;

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
    void saveHighScore();
    void showMessage(const QString &text, double seconds);

    // ---- simulation ----
    double currentSpeed() const;
    void updateBalls(double dt);
    void updatePowerUps(double dt);
    void updateLasers(double dt);
    void updateParticles(double dt);
    void launchBalls();
    void fireLasers();
    void spawnLaser(double x);
    void spawnBall(double x, double y, double vx, double vy);
    void spawnPowerUp(double x, double y);
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

    // ---- drawing (all into the small pixel canvas) ----
    void renderFrame();
    void drawHud(QPainter &p);
};

#endif // GAMEWIDGET_H
