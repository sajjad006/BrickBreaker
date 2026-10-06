#include "gamewidget.h"

#include <QPainter>
#include <QBrush>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QPaintEvent>
#include <QSettings>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <ctime>

namespace {

// ---------------------------------------------------------------------------
// Pixel grid. 1 tile = 8 x 8 pixels. The canvas is 26 x 30 tiles = 208 x 240.
//
//   tile rows 0-1   : HUD (score, lives ...)
//   tile row  2     : solid top wall
//   tile col  0/25  : solid side walls
//   tile rows 5-12  : bricks (each brick is 2 tiles wide, 1 tile high)
//   tile row  28    : paddle
// ---------------------------------------------------------------------------
const int TILE   = 8;
const int GRID_W = 26;
const int GRID_H = 30;
const int W      = GRID_W * TILE;     // 208
const int H      = GRID_H * TILE;     // 240
const int HUD_H  = 2 * TILE;          // 16
const int LEFT   = TILE;              // inner edge of the left wall   (8)
const int RIGHT  = W - TILE;          // inner edge of the right wall  (200)
const int TOP    = HUD_H + TILE;      // underside of the top wall     (24)

const int BRICK_W  = 16;
const int BRICK_H  = 8;
const int BRICK_X0 = LEFT;
const int BRICK_Y0 = 5 * TILE;        // 40

const double PADDLE_Y        = 224.0;
const double PADDLE_H        = 5.0;
const double PADDLE_W_BASE   = 32.0;
const double PADDLE_W_WIDE   = 48.0;
const double PADDLE_SPEED    = 170.0;
const double BALL_R          = 2.0;   // ball is a 4 x 4 square of pixels

const int NUM_LEVELS = 6;
const int MAX_LIVES  = 5;

// Level layouts, 12 characters per row:
//   '.' empty      '1'..'3' brick that needs that many hits
//   '#' solid brick (cannot be broken)
//   'H' HEART brick: gives +1 life. It is always walled into a top corner
//       with solid bricks, so the ball can only reach it from above - that
//       is the hardest spot in every level. It is optional (you do not
//       have to break it to clear the level).
const char *const LEVELS[NUM_LEVELS][8] = {
    { // 1 - warm up
        "H#1111111111",
        "#11111111111",
        "222222222222",
        "222222222222",
        "111111111111",
        "............",
        "............",
        "............"
    },
    { // 2 - stripes
        "1111111111#H",
        "22222222222#",
        "1.1.1.1.1.1.",
        "333333333333",
        "222222222222",
        "1.1.1.1.1.1.",
        "111111111111",
        "............"
    },
    { // 3 - checkerboard
        "H#1212121212",
        "#12121212121",
        "121212121212",
        "232323232323",
        "323232323232",
        "232323232323",
        "............",
        "............"
    },
    { // 4 - pillars
        "2222222222#H",
        "22222222222#",
        "2#2222222#22",
        "223333333322",
        "2#33333333#2",
        "222222222222",
        "111111111111",
        "............"
    },
    { // 5 - diamond
        "H#3333333333",
        "#33322223333",
        "333221122333",
        "322111111223",
        "332211112233",
        "333222222333",
        "#3333333333#",
        "............"
    },
    { // 6 - fortress
        "3333333333#H",
        "3#3#33#3#33#",
        "333333333333",
        "2.2.22.2.2.2",
        "222222222222",
        "#.#.##.#.#.#",
        "111111111111",
        "............"
    }
};

double rnd() { return std::rand() / double(RAND_MAX); }

double clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

double mouseXOf(QMouseEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().x();
#else
    return e->x();
#endif
}

int brickLeft(int c) { return BRICK_X0 + c * BRICK_W; }
int brickTop(int r)  { return BRICK_Y0 + r * BRICK_H; }

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------

QColor powerColor(int type)
{
    switch (type) {
    case 0:  return QColor(80, 140, 255);   // wide
    case 1:  return QColor(80, 220, 220);   // slow
    case 2:  return QColor(255, 220, 80);   // multi-ball
    case 3:  return QColor(255, 120, 60);   // laser
    default: return QColor(150, 230, 90);   // sticky
    }
}

char powerLetter(int type)
{
    switch (type) {
    case 0:  return 'W';
    case 1:  return 'S';
    case 2:  return 'M';
    case 3:  return 'L';
    default: return 'G';
    }
}

QColor rowColor(int r)
{
    static const unsigned pal[8] = {
        0x3ec6e0, 0x4cd964, 0xd8e03e, 0xb06bff,
        0xff7ab8, 0x3e8bff, 0x7be0a0, 0xe0a03e
    };
    return QColor::fromRgb(pal[r & 7]);
}

const QColor HEART_PINK(255, 90, 130);

// ---------------------------------------------------------------------------
// Tiny bitmap font: every letter is 3 x 5 pixels (15 characters, row by row)
// ---------------------------------------------------------------------------

struct Glyph { char ch; const char *bits; };

const Glyph GLYPHS[] = {
    {'A', "010101111101101"}, {'B', "110101110101110"}, {'C', "011100100100011"},
    {'D', "110101101101110"}, {'E', "111100110100111"}, {'F', "111100110100100"},
    {'G', "011100101101011"}, {'H', "101101111101101"}, {'I', "111010010010111"},
    {'J', "001001001101010"}, {'K', "101101110101101"}, {'L', "100100100100111"},
    {'M', "101111111101101"}, {'N', "110101101101101"}, {'O', "010101101101010"},
    {'P', "110101110100100"}, {'Q', "010101101111011"}, {'R', "110101110101101"},
    {'S', "011100010001110"}, {'T', "111010010010010"}, {'U', "101101101101111"},
    {'V', "101101101101010"}, {'W', "101101111111101"}, {'X', "101101010101101"},
    {'Y', "101101010010010"}, {'Z', "111001010100111"},
    {'0', "111101101101111"}, {'1', "010110010010111"}, {'2', "110001010100111"},
    {'3', "110001010001110"}, {'4', "101101111001001"}, {'5', "111100110001110"},
    {'6', "011100111101111"}, {'7', "111001010010010"}, {'8', "111101111101111"},
    {'9', "111101111001110"},
    {':', "000010000010000"}, {'-', "000000111000000"}, {'+', "000010111010000"},
    {'/', "001001010100100"}, {'!', "010010010000010"}, {'.', "000000000000010"},
    {'=', "000111000111000"}
};
const int NUM_GLYPHS = int(sizeof(GLYPHS) / sizeof(GLYPHS[0]));

const char *glyphBits(char ch)
{
    ch = char(std::toupper((unsigned char)ch));
    for (int i = 0; i < NUM_GLYPHS; ++i)
        if (GLYPHS[i].ch == ch) return GLYPHS[i].bits;
    return nullptr;                     // space / unknown: draw nothing
}

int textWidth(const QString &s, int scale)
{
    if (s.isEmpty()) return 0;
    return s.length() * 4 * scale - scale;
}

void drawText(QPainter &p, const QString &s, int x, int y, int scale, const QColor &c)
{
    for (int i = 0; i < s.length(); ++i) {
        const char *bits = glyphBits(s.at(i).toLatin1());
        if (bits) {
            for (int r = 0; r < 5; ++r)
                for (int col = 0; col < 3; ++col)
                    if (bits[r * 3 + col] == '1')
                        p.fillRect(x + col * scale, y + r * scale, scale, scale, c);
        }
        x += 4 * scale;
    }
}

// centered on the canvas, with a 1 pixel dark shadow so it reads on any background
void drawTextCentered(QPainter &p, const QString &s, int y, int scale, const QColor &c)
{
    const int x = (W - textWidth(s, scale)) / 2;
    drawText(p, s, x + 1, y + 1, scale, QColor(0, 0, 0));
    drawText(p, s, x, y, scale, c);
}

// ---------------------------------------------------------------------------
// Small sprites ('#' = pixel)
// ---------------------------------------------------------------------------

const char *const HEART[6] = {
    ".##.##.",
    "#######",
    "#######",
    ".#####.",
    "..###..",
    "...#..."
};

void drawSprite(QPainter &p, const char *const *rows, int h, int x, int y, const QColor &c)
{
    for (int r = 0; r < h; ++r)
        for (int i = 0; rows[r][i] != '\0'; ++i)
            if (rows[r][i] == '#')
                p.fillRect(x + i, y + r, 1, 1, c);
}

void drawHeart(QPainter &p, int x, int y)
{
    drawSprite(p, HEART, 6, x, y, HEART_PINK);
    p.fillRect(x + 1, y + 1, 1, 1, QColor(255, 220, 230));   // little shine pixel
}

// One solid wall tile (8 x 8)
void drawWallTile(QPainter &p, int x, int y)
{
    p.fillRect(x, y, TILE, TILE, QColor(0x5a, 0x60, 0x76));
    p.fillRect(x, y, TILE - 1, 1, QColor(0x8a, 0x92, 0xab));         // light top edge
    p.fillRect(x, y, 1, TILE - 1, QColor(0x8a, 0x92, 0xab));         // light left edge
    p.fillRect(x, y + TILE - 1, TILE, 1, QColor(0x33, 0x38, 0x4c));  // dark bottom edge
    p.fillRect(x + TILE - 1, y, 1, TILE, QColor(0x33, 0x38, 0x4c));  // dark right edge
    p.fillRect(x + 3, y + 3, 2, 2, QColor(0x45, 0x4b, 0x60));        // rivet
}

// Dark checker pattern over the playfield (looks pixelated, unlike a fade).
// The HUD strip is left alone so score and lives stay readable.
void dimScreen(QPainter &p)
{
    p.fillRect(0, HUD_H, W, H - HUD_H, QBrush(QColor(0, 0, 0), Qt::Dense3Pattern));
}

void drawOverlay(QPainter &p, const QString &title, const QString &l1,
                 const QString &l2, const QString &l3)
{
    dimScreen(p);
    drawTextCentered(p, title, 60, 3, QColor(255, 220, 90));
    drawTextCentered(p, l1, 100, 2, QColor(255, 255, 255));
    drawTextCentered(p, l2, 120, 2, QColor(200, 210, 230));
    drawTextCentered(p, l3, 150, 1, QColor(200, 210, 230));
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent)
{
    std::srand(unsigned(std::time(nullptr)));

    setWindowTitle("Brick Breaker - Pixel");
    setMinimumSize(W, H);
    resize(W * 3, H * 3);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    frame = QImage(W, H, QImage::Format_RGB32);

    QSettings settings;
    highScore = settings.value("highScore", 0).toInt();

    for (int i = 0; i < MAX_BALLS; ++i)     balls[i].active = false;
    for (int i = 0; i < MAX_POWERUPS; ++i)  powerups[i].active = false;
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].active = false;
    for (int i = 0; i < MAX_LASERS; ++i)    lasers[i].active = false;

    paddleX = W / 2.0;
    paddleW = PADDLE_W_BASE;
    loadLevel(1);
    resetBallOnPaddle();

    connect(&timer, &QTimer::timeout, this, &GameWidget::tick);
    timer.start(16);
    clock.start();
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void GameWidget::newGame()
{
    score = 0;
    lives = 3;
    level = 1;
    combo = 0;
    paddleX = W / 2.0;
    paddleW = PADDLE_W_BASE;
    resetEffects();
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].active = false;
    loadLevel(1);
    resetBallOnPaddle();
    state = Ready;
}

void GameWidget::loadLevel(int lvl)
{
    const char *const *layout = LEVELS[(lvl - 1) % NUM_LEVELS];
    bricksLeft = 0;
    bricksBroken = 0;

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            const char ch = layout[r][c];
            Brick &b = bricks[r][c];
            b.alive = false;
            b.solid = false;
            b.heart = false;
            b.hp = 0;
            b.maxHp = 0;
            if (ch >= '1' && ch <= '3') {
                b.alive = true;
                b.hp = b.maxHp = ch - '0';
                ++bricksLeft;
            } else if (ch == '#') {
                b.alive = true;
                b.solid = true;
                b.hp = b.maxHp = 1;
            } else if (ch == 'H') {
                b.alive = true;
                b.heart = true;
                b.hp = b.maxHp = 1;
            }
        }
    }
    for (int i = 0; i < MAX_POWERUPS; ++i) powerups[i].active = false;
    for (int i = 0; i < MAX_LASERS; ++i)   lasers[i].active = false;
}

void GameWidget::startNextLevel()
{
    ++level;
    combo = 0;
    resetEffects();
    loadLevel(level);
    resetBallOnPaddle();
    state = Ready;
}

void GameWidget::levelComplete()
{
    lastBonus = lives * 100 + level * 50;
    score += lastBonus;
    if (score > highScore) highScore = score;

    for (int i = 0; i < MAX_BALLS; ++i)    balls[i].active = false;
    for (int i = 0; i < MAX_POWERUPS; ++i) powerups[i].active = false;
    for (int i = 0; i < MAX_LASERS; ++i)   lasers[i].active = false;

    if (level >= NUM_LEVELS) {
        state = Won;
        saveHighScore();
    } else {
        state = LevelClear;
    }
}

void GameWidget::loseLife()
{
    --lives;
    combo = 0;
    shakeTimer = 0.3;
    resetEffects();
    for (int i = 0; i < MAX_POWERUPS; ++i) powerups[i].active = false;
    for (int i = 0; i < MAX_LASERS; ++i)   lasers[i].active = false;

    if (lives <= 0) {
        state = GameOver;
        saveHighScore();
    } else {
        resetBallOnPaddle();
        state = Ready;
    }
}

void GameWidget::grantLife()
{
    if (lives < MAX_LIVES) {
        ++lives;
        showMessage("EXTRA LIFE!", 2.0);
    } else {
        score += 200;
        showMessage("LIFE BONUS +200", 2.0);
    }
}

void GameWidget::resetBallOnPaddle()
{
    for (int i = 0; i < MAX_BALLS; ++i) balls[i].active = false;
    Ball &b = balls[0];
    b.active = true;
    b.stuck = true;
    b.stuckOffset = 0.0;
    b.x = paddleX;
    b.y = PADDLE_Y - BALL_R;
    b.vx = 0.0;
    b.vy = 0.0;
}

void GameWidget::resetEffects()
{
    wideTimer = 0.0;
    slowTimer = 0.0;
    laserTimer = 0.0;
    stickyTimer = 0.0;
    laserCooldown = 0.0;
}

void GameWidget::primaryAction()
{
    switch (state) {
    case Title:
    case GameOver:
    case Won:
        newGame();
        break;
    case LevelClear:
        startNextLevel();
        break;
    case Ready:
        launchBalls();
        state = Playing;
        break;
    case Playing:
        launchBalls();      // releases sticky balls
        fireLasers();       // does nothing unless the laser power-up is active
        break;
    case Paused:
        break;
    }
}

void GameWidget::togglePause()
{
    if (state == Playing)
        state = Paused;
    else if (state == Paused)
        state = Playing;
}

void GameWidget::saveHighScore()
{
    QSettings settings;
    if (score > settings.value("highScore", 0).toInt())
        settings.setValue("highScore", score);
}

void GameWidget::showMessage(const QString &text, double seconds)
{
    message = text;
    messageTimer = seconds;
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void GameWidget::tick()
{
    double dt = clock.restart() / 1000.0;
    if (dt > 0.04) dt = 0.04;

    if (state == Ready || state == Playing) {
        double move = 0.0;
        if (leftHeld)  move -= 1.0;
        if (rightHeld) move += 1.0;
        paddleX += move * PADDLE_SPEED * dt;

        const double target = (wideTimer > 0.0) ? PADDLE_W_WIDE : PADDLE_W_BASE;
        paddleW += (target - paddleW) * clampd(dt * 10.0, 0.0, 1.0);
        paddleX = clampd(paddleX, LEFT + paddleW / 2.0, RIGHT - paddleW / 2.0);
    }

    if (state == Playing) {
        if (wideTimer > 0.0)     wideTimer = std::fmax(0.0, wideTimer - dt);
        if (slowTimer > 0.0)     slowTimer = std::fmax(0.0, slowTimer - dt);
        if (laserTimer > 0.0)    laserTimer = std::fmax(0.0, laserTimer - dt);
        if (stickyTimer > 0.0)   stickyTimer = std::fmax(0.0, stickyTimer - dt);
        if (laserCooldown > 0.0) laserCooldown = std::fmax(0.0, laserCooldown - dt);

        if (fireHeld) fireLasers();

        // when the glue wears off, a ball still stuck to the paddle is let go
        if (stickyTimer <= 0.0) launchBalls();
    }

    if (state == Ready || state == Playing)
        updateBalls(dt);

    if (state == Playing) {
        updateLasers(dt);
        updatePowerUps(dt);
    }

    if (state != Paused)
        updateParticles(dt);

    if (shakeTimer > 0.0)   shakeTimer = std::fmax(0.0, shakeTimer - dt);
    if (messageTimer > 0.0) messageTimer = std::fmax(0.0, messageTimer - dt);

    update();
}

double GameWidget::currentSpeed() const
{
    double boost = bricksBroken * 0.5;
    if (boost > 35.0) boost = 35.0;
    double sp = 92.0 + (level - 1) * 8.0 + boost;     // pixels per second
    if (slowTimer > 0.0) sp *= 0.7;
    return sp;
}

int GameWidget::activeBallCount() const
{
    int n = 0;
    for (int i = 0; i < MAX_BALLS; ++i)
        if (balls[i].active) ++n;
    return n;
}

void GameWidget::updateBalls(double dt)
{
    const double spd = currentSpeed();

    for (int i = 0; i < MAX_BALLS; ++i) {
        Ball &b = balls[i];
        if (!b.active) continue;

        if (b.stuck) {
            b.stuckOffset = clampd(b.stuckOffset, -paddleW / 2.0, paddleW / 2.0);
            b.x = paddleX + b.stuckOffset;
            b.y = PADDLE_Y - BALL_R;
            continue;
        }

        // Keep the ball at the current speed (handles slow-motion / level speed-up)
        double len = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        if (len < 1e-6) { b.vx = 0.0; b.vy = -spd; len = spd; }
        b.vx *= spd / len;
        b.vy *= spd / len;

        // Never let the ball travel almost horizontally
        const double minVy = spd * 0.25;
        if (std::fabs(b.vy) < minVy) {
            b.vy = (b.vy < 0.0 ? -minVy : minVy);
            const double vxAbs = std::sqrt(spd * spd - b.vy * b.vy);
            b.vx = (b.vx < 0.0 ? -vxAbs : vxAbs);
        }

        // Move in steps of at most 1 pixel so the ball cannot skip through bricks
        const int steps = int(spd * dt) + 1;
        const double sdt = dt / steps;

        for (int s = 0; s < steps; ++s) {
            b.x += b.vx * sdt;
            b.y += b.vy * sdt;

            // solid walls (left, right, top)
            if (b.x < LEFT + BALL_R)       { b.x = LEFT + BALL_R;   b.vx =  std::fabs(b.vx); }
            else if (b.x > RIGHT - BALL_R) { b.x = RIGHT - BALL_R;  b.vx = -std::fabs(b.vx); }
            if (b.y < TOP + BALL_R)        { b.y = TOP + BALL_R;    b.vy =  std::fabs(b.vy); }

            // fell out of the playfield
            if (b.y - BALL_R > H) {
                b.active = false;
                break;
            }

            // paddle
            if (b.vy > 0.0
                    && b.y + BALL_R >= PADDLE_Y
                    && b.y <= PADDLE_Y + PADDLE_H * 0.5
                    && b.x >= paddleX - paddleW / 2.0 - BALL_R * 0.6
                    && b.x <= paddleX + paddleW / 2.0 + BALL_R * 0.6) {
                double t = clampd((b.x - paddleX) / (paddleW / 2.0), -1.0, 1.0);
                // up to ~60 degrees, plus a tiny random wobble so the ball can
                // never get stuck bouncing straight up and down in one column
                const double ang = t * 1.05 + (rnd() - 0.5) * 0.14;
                b.vx = spd * std::sin(ang);
                b.vy = -spd * std::cos(ang);
                b.y = PADDLE_Y - BALL_R;
                combo = 0;
                if (stickyTimer > 0.0) {
                    b.stuck = true;
                    b.stuckOffset = b.x - paddleX;
                    b.vx = 0.0;
                    b.vy = 0.0;
                    break;
                }
            }

            // bricks
            bool hit = false;
            for (int r = 0; r < ROWS && !hit; ++r) {
                for (int c = 0; c < COLS && !hit; ++c) {
                    if (!bricks[r][c].alive) continue;
                    const double left = brickLeft(c);
                    const double top  = brickTop(r);
                    const double nx = clampd(b.x, left, left + BRICK_W);
                    const double ny = clampd(b.y, top,  top + BRICK_H);
                    const double dx = b.x - nx;
                    const double dy = b.y - ny;
                    const double d2 = dx * dx + dy * dy;
                    if (d2 < BALL_R * BALL_R) {
                        const double d = std::sqrt(d2);
                        if (d > 1e-6) {
                            const double ux = dx / d;
                            const double uy = dy / d;
                            b.x += ux * (BALL_R - d + 0.05);
                            b.y += uy * (BALL_R - d + 0.05);
                            if (std::fabs(ux) > std::fabs(uy))
                                b.vx = (ux > 0.0 ? 1.0 : -1.0) * std::fabs(b.vx);
                            else
                                b.vy = (uy > 0.0 ? 1.0 : -1.0) * std::fabs(b.vy);
                        } else {
                            b.vy = -b.vy;
                        }
                        hitBrick(r, c, 1);
                        hit = true;
                    }
                }
            }
            if (state != Playing && state != Ready) return;
        }
    }

    if (state == Playing && activeBallCount() == 0)
        loseLife();
}

void GameWidget::hitBrick(int r, int c, int damage)
{
    Brick &br = bricks[r][c];
    const double cx = brickLeft(c) + BRICK_W / 2.0;
    const double cy = brickTop(r) + BRICK_H / 2.0;

    if (br.solid) {                                   // solid brick: just a few sparks
        spawnParticles(cx, cy, QColor(0x9a, 0xa0, 0xb4), 3);
        return;
    }

    if (br.heart) {                                   // the extra-life brick
        br.hp -= damage;
        if (br.hp <= 0) {
            br.alive = false;
            score += 100;
            spawnParticles(cx, cy, HEART_PINK, 30);
            grantLife();
            if (score > highScore) highScore = score;
        }
        return;
    }

    const QColor col = brickColor(r, c);
    br.hp -= damage;
    ++combo;
    int mult = 1 + combo / 4;
    if (mult > 5) mult = 5;

    if (br.hp <= 0) {
        br.alive = false;
        --bricksLeft;
        ++bricksBroken;
        score += 10 * br.maxHp * mult;
        spawnParticles(cx, cy, col, 14);
        if (rnd() < 0.18)
            spawnPowerUp(cx, cy);
    } else {
        score += 5 * mult;
        spawnParticles(cx, cy, col, 4);
    }

    if (score > highScore) highScore = score;
    if (bricksLeft <= 0) levelComplete();
}

void GameWidget::launchBalls()
{
    const double spd = currentSpeed();
    for (int i = 0; i < MAX_BALLS; ++i) {
        Ball &b = balls[i];
        if (!b.active || !b.stuck) continue;
        b.stuck = false;
        double a = clampd(b.stuckOffset / (paddleW / 2.0), -1.0, 1.0) * 0.9;
        if (std::fabs(a) < 0.1) a = (rnd() - 0.5) * 0.5;
        b.vx = spd * std::sin(a);
        b.vy = -spd * std::cos(a);
    }
}

void GameWidget::fireLasers()
{
    if (laserTimer <= 0.0 || laserCooldown > 0.0) return;
    spawnLaser(paddleX - paddleW / 2.0 + 3.0);
    spawnLaser(paddleX + paddleW / 2.0 - 4.0);
    laserCooldown = 0.25;
}

void GameWidget::spawnLaser(double x)
{
    for (int i = 0; i < MAX_LASERS; ++i) {
        if (!lasers[i].active) {
            lasers[i].active = true;
            lasers[i].x = x;
            lasers[i].y = PADDLE_Y - 2.0;
            return;
        }
    }
}

void GameWidget::spawnBall(double x, double y, double vx, double vy)
{
    for (int i = 0; i < MAX_BALLS; ++i) {
        if (!balls[i].active) {
            balls[i].active = true;
            balls[i].stuck = false;
            balls[i].stuckOffset = 0.0;
            balls[i].x = x;
            balls[i].y = y;
            balls[i].vx = vx;
            balls[i].vy = vy;
            return;
        }
    }
}

void GameWidget::spawnPowerUp(double x, double y)
{
    for (int i = 0; i < MAX_POWERUPS; ++i) {
        if (powerups[i].active) continue;
        const int roll = std::rand() % 100;
        int type;
        if (roll < 25)      type = PU_WIDE;
        else if (roll < 45) type = PU_SLOW;
        else if (roll < 70) type = PU_MULTI;
        else if (roll < 85) type = PU_LASER;
        else                type = PU_STICKY;
        powerups[i].active = true;
        powerups[i].x = x;
        powerups[i].y = y;
        powerups[i].type = type;
        return;
    }
}

void GameWidget::spawnParticles(double x, double y, const QColor &col, int n)
{
    for (int i = 0; i < MAX_PARTICLES && n > 0; ++i) {
        Particle &q = particles[i];
        if (q.active) continue;
        const double a = rnd() * 6.2831853;
        const double sp = 15.0 + rnd() * 50.0;
        q.active = true;
        q.x = x;
        q.y = y;
        q.vx = std::cos(a) * sp;
        q.vy = std::sin(a) * sp - 12.0;
        q.maxLife = 0.35 + rnd() * 0.4;
        q.life = q.maxLife;
        q.color = col;
        --n;
    }
}

void GameWidget::applyPowerUp(int type)
{
    switch (type) {
    case PU_WIDE:
        wideTimer = 12.0;
        showMessage("WIDE PADDLE", 1.5);
        break;
    case PU_SLOW:
        slowTimer = 10.0;
        showMessage("SLOW BALL", 1.5);
        break;
    case PU_MULTI: {
        Ball src[MAX_BALLS];
        for (int i = 0; i < MAX_BALLS; ++i) src[i] = balls[i];
        const double spd = currentSpeed();
        for (int i = 0; i < MAX_BALLS; ++i) {
            if (!src[i].active) continue;
            double vx = src[i].vx;
            double vy = src[i].vy;
            if (src[i].stuck) { vx = 0.0; vy = -spd; }
            for (int k = -1; k <= 1; k += 2) {
                const double a = 0.4 * k;
                const double nvx = vx * std::cos(a) - vy * std::sin(a);
                const double nvy = vx * std::sin(a) + vy * std::cos(a);
                spawnBall(src[i].x, src[i].y, nvx, nvy);
            }
        }
        showMessage("MULTI BALL", 1.5);
        break;
    }
    case PU_LASER:
        laserTimer = 10.0;
        showMessage("LASERS: HOLD SPACE", 2.0);
        break;
    case PU_STICKY:
        stickyTimer = 12.0;
        showMessage("STICKY: SPACE RELEASES", 2.0);
        break;
    default:
        break;
    }
}

void GameWidget::updatePowerUps(double dt)
{
    for (int i = 0; i < MAX_POWERUPS; ++i) {
        PowerUp &pu = powerups[i];
        if (!pu.active) continue;
        pu.y += 45.0 * dt;

        if (pu.y > H + 10) {
            pu.active = false;
            continue;
        }
        if (pu.y + 3.5 >= PADDLE_Y && pu.y - 3.5 <= PADDLE_Y + PADDLE_H
                && pu.x >= paddleX - paddleW / 2.0 - 6.5
                && pu.x <= paddleX + paddleW / 2.0 + 6.5) {
            pu.active = false;
            score += 50;
            if (score > highScore) highScore = score;
            applyPowerUp(pu.type);
        }
    }
}

void GameWidget::updateLasers(double dt)
{
    for (int i = 0; i < MAX_LASERS; ++i) {
        Laser &l = lasers[i];
        if (!l.active) continue;
        l.y -= 180.0 * dt;
        if (l.y < TOP) {
            l.active = false;
            continue;
        }
        bool hit = false;
        for (int r = 0; r < ROWS && !hit; ++r) {
            for (int c = 0; c < COLS && !hit; ++c) {
                if (!bricks[r][c].alive) continue;
                if (l.x >= brickLeft(c) && l.x < brickLeft(c) + BRICK_W
                        && l.y >= brickTop(r) && l.y < brickTop(r) + BRICK_H) {
                    l.active = false;
                    hit = true;
                    hitBrick(r, c, 1);
                }
            }
        }
        if (state != Playing) return;
    }
}

void GameWidget::updateParticles(double dt)
{
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle &q = particles[i];
        if (!q.active) continue;
        q.life -= dt;
        if (q.life <= 0.0) {
            q.active = false;
            continue;
        }
        q.vy += 90.0 * dt;
        q.x += q.vx * dt;
        q.y += q.vy * dt;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QColor GameWidget::brickColor(int r, int c) const
{
    const Brick &b = bricks[r][c];
    if (b.solid) return QColor(0x7a, 0x80, 0x92);
    if (b.heart) return QColor(0xd0, 0x40, 0x70);
    if (b.hp >= 3) return QColor(0xd9, 0x43, 0x3f);
    if (b.hp == 2) return QColor(0xe8, 0x9a, 0x2c);
    return rowColor(r);
}

// Whole-number zoom only, so every canvas pixel becomes a crisp square block
int GameWidget::viewScale() const
{
    const int sx = width() / W;
    const int sy = height() / H;
    const int s = sx < sy ? sx : sy;
    return s < 1 ? 1 : s;
}

int GameWidget::viewOffsetX() const { return (width()  - W * viewScale()) / 2; }
int GameWidget::viewOffsetY() const { return (height() - H * viewScale()) / 2; }

double GameWidget::toLogicalX(double widgetX) const
{
    return (widgetX - viewOffsetX()) / double(viewScale());
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void GameWidget::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        leftHeld = true;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        rightHeld = true;
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!e->isAutoRepeat()) {
            fireHeld = true;
            primaryAction();
        }
        break;
    case Qt::Key_P:
    case Qt::Key_Escape:
        if (!e->isAutoRepeat()) togglePause();
        break;
    case Qt::Key_R:
        if (state != Title) newGame();
        break;
    default:
        QWidget::keyPressEvent(e);
    }
}

void GameWidget::keyReleaseEvent(QKeyEvent *e)
{
    if (e->isAutoRepeat()) return;
    switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        leftHeld = false;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        rightHeld = false;
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        fireHeld = false;
        break;
    default:
        QWidget::keyReleaseEvent(e);
    }
}

void GameWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (state == Ready || state == Playing) {
        const double mx = toLogicalX(mouseXOf(e));
        paddleX = clampd(mx, LEFT + paddleW / 2.0, RIGHT - paddleW / 2.0);
    }
}

void GameWidget::mousePressEvent(QMouseEvent *)
{
    primaryAction();
}

void GameWidget::focusOutEvent(QFocusEvent *e)
{
    leftHeld = rightHeld = fireHeld = false;
    if (state == Playing) state = Paused;
    QWidget::focusOutEvent(e);
}

// ---------------------------------------------------------------------------
// Drawing: everything goes into the small canvas with fillRect only
// ---------------------------------------------------------------------------

void GameWidget::drawHud(QPainter &p)
{
    p.fillRect(0, 0, W, HUD_H, QColor(8, 9, 20));

    drawText(p, QString("SCORE ") + QString::number(score), 4, 2, 1, QColor(255, 255, 255));
    drawText(p, QString("BEST ") + QString::number(highScore), 84, 2, 1, QColor(150, 160, 190));

    // lives as pixel hearts (right side)
    for (int i = 0; i < lives; ++i)
        drawHeart(p, W - 11 - i * 9, 2);

    drawText(p, QString("LV ") + QString::number(level) + "/" + QString::number(NUM_LEVELS),
             4, 9, 1, QColor(200, 210, 230));

    int mult = 1 + combo / 4;
    if (mult > 5) mult = 5;
    if (mult > 1)
        drawText(p, QString("X") + QString::number(mult), 48, 9, 1, QColor(255, 210, 80));

    // active power-ups: letter + seconds left
    struct Fx { char letter; double t; int type; };
    const Fx fx[4] = {
        { 'W', wideTimer,   PU_WIDE },
        { 'S', slowTimer,   PU_SLOW },
        { 'L', laserTimer,  PU_LASER },
        { 'G', stickyTimer, PU_STICKY }
    };
    int ex = 84;
    for (int i = 0; i < 4; ++i) {
        if (fx[i].t <= 0.0) continue;
        drawText(p, QString(QChar(fx[i].letter)) + QString::number(int(std::ceil(fx[i].t))),
                 ex, 9, 1, powerColor(fx[i].type));
        ex += 24;
    }
}

void GameWidget::renderFrame()
{
    QPainter p(&frame);                  // antialiasing stays off: hard pixel edges

    // background + faint grid dots on every tile corner
    p.fillRect(0, 0, W, H, QColor(14, 16, 34));
    for (int ty = 3; ty < GRID_H; ++ty)
        for (int tx = 1; tx < GRID_W - 1; ++tx)
            p.fillRect(tx * TILE, ty * TILE, 1, 1, QColor(34, 38, 68));

    // solid walls: top row and both sides
    for (int tx = 0; tx < GRID_W; ++tx)
        drawWallTile(p, tx * TILE, HUD_H);
    for (int ty = 3; ty < GRID_H; ++ty) {
        drawWallTile(p, 0, ty * TILE);
        drawWallTile(p, (GRID_W - 1) * TILE, ty * TILE);
    }

    // bricks (16 x 8 pixels each, sitting exactly on the grid)
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            const Brick &b = bricks[r][c];
            if (!b.alive) continue;
            const int x = brickLeft(c);
            const int y = brickTop(r);
            const QColor col = brickColor(r, c);

            p.fillRect(x, y, BRICK_W, BRICK_H, col.darker(170));           // grid line / shadow
            p.fillRect(x, y, BRICK_W - 1, BRICK_H - 1, col);                // body
            p.fillRect(x, y, BRICK_W - 1, 1, col.lighter(145));             // top highlight

            if (b.solid) {
                // diagonal hatch
                for (int i = 0; i < 6; ++i) {
                    p.fillRect(x + 3 + i * 2, y + 1 + (i % 2) * 2, 1, 1, QColor(0x4a, 0x50, 0x62));
                    p.fillRect(x + 4 + i * 2, y + 3 + (i % 2) * 2, 1, 1, QColor(0xa8, 0xae, 0xc0));
                }
            } else if (b.heart) {
                drawHeart(p, x + 4, y + 1);
            } else if (b.hp < b.maxHp) {
                // cracks on damaged bricks
                const QColor crack(0, 0, 0, 160);
                p.fillRect(x + 4, y + 1, 1, 1, crack);
                p.fillRect(x + 5, y + 2, 1, 1, crack);
                p.fillRect(x + 6, y + 3, 1, 1, crack);
                p.fillRect(x + 7, y + 4, 1, 1, crack);
                p.fillRect(x + 8, y + 5, 1, 1, crack);
            }
        }
    }

    // paddle
    {
        const int pw = int(paddleW + 0.5) / 2 * 2;
        const int px = int(paddleX + 0.5) - pw / 2;
        const int py = int(PADDLE_Y);
        p.fillRect(px + 1, py, pw - 2, 5, QColor(80, 100, 190));           // body
        p.fillRect(px, py + 1, pw, 3, QColor(80, 100, 190));               // rounded ends
        p.fillRect(px + 1, py, pw - 2, 1, QColor(200, 214, 255));          // highlight
        p.fillRect(px + 1, py + 4, pw - 2, 1, QColor(40, 50, 120));        // shadow
        if (laserTimer > 0.0) {
            p.fillRect(px + 1, py - 3, 2, 3, QColor(255, 120, 60));
            p.fillRect(px + pw - 3, py - 3, 2, 3, QColor(255, 120, 60));
        }
        if (stickyTimer > 0.0)
            p.fillRect(px + 2, py - 1, pw - 4, 1, QColor(150, 230, 90));
    }

    // lasers
    for (int i = 0; i < MAX_LASERS; ++i) {
        if (!lasers[i].active) continue;
        p.fillRect(int(lasers[i].x), int(lasers[i].y), 1, 5, QColor(255, 150, 90));
        p.fillRect(int(lasers[i].x), int(lasers[i].y), 1, 1, QColor(255, 255, 255));
    }

    // power-up capsules (13 x 7 pixels, letter inside)
    for (int i = 0; i < MAX_POWERUPS; ++i) {
        const PowerUp &pu = powerups[i];
        if (!pu.active) continue;
        const QColor col = powerColor(pu.type);
        const int x = int(pu.x) - 6;
        const int y = int(pu.y) - 3;
        p.fillRect(x + 1, y, 11, 7, col.lighter(150));
        p.fillRect(x, y + 1, 13, 5, col.lighter(150));
        p.fillRect(x + 1, y + 1, 11, 5, col.darker(170));
        drawText(p, QString(QChar(powerLetter(pu.type))), x + 5, y + 1, 1, QColor(255, 255, 255));
    }

    // balls (4 x 4 pixel squares)
    for (int i = 0; i < MAX_BALLS; ++i) {
        const Ball &b = balls[i];
        if (!b.active) continue;
        const int x = int(std::floor(b.x + 0.5)) - 2;
        const int y = int(std::floor(b.y + 0.5)) - 2;
        p.fillRect(x, y, 4, 4, QColor(255, 255, 255));
        p.fillRect(x + 3, y + 3, 1, 1, QColor(160, 190, 255));
    }

    // particles (square pixels)
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle &q = particles[i];
        if (!q.active) continue;
        const int size = (q.life > q.maxLife * 0.4) ? 2 : 1;
        p.fillRect(int(q.x), int(q.y), size, size, q.color);
    }

    drawHud(p);

    // floating message
    if (messageTimer > 0.0 && state == Playing)
        drawTextCentered(p, message, 170, 2, QColor(255, 240, 150));

    // state screens
    switch (state) {
    case Title:
        dimScreen(p);
        drawTextCentered(p, "BRICK BREAKER", 40, 3, QColor(255, 220, 90));
        drawTextCentered(p, "PRESS SPACE OR CLICK TO START", 82, 1, QColor(255, 255, 255));
        drawTextCentered(p, "MOUSE OR ARROWS OR A/D: MOVE", 100, 1, QColor(200, 210, 230));
        drawTextCentered(p, "SPACE: LAUNCH / FIRE LASERS", 110, 1, QColor(200, 210, 230));
        drawTextCentered(p, "P: PAUSE    R: RESTART", 120, 1, QColor(200, 210, 230));
        drawTextCentered(p, "POWER-UPS", 140, 1, QColor(255, 220, 90));
        drawTextCentered(p, "W WIDE   S SLOW   M MULTI", 152, 1, QColor(200, 210, 230));
        drawTextCentered(p, "L LASER   G GLUE", 162, 1, QColor(200, 210, 230));
        drawHeart(p, 56, 181);
        drawTextCentered(p, "= EXTRA LIFE BRICK", 182, 1, HEART_PINK);
        drawTextCentered(p, "HIDDEN IN A WALLED CORNER", 194, 1, QColor(170, 190, 230));
        break;
    case Ready:
        drawTextCentered(p, QString("LEVEL ") + QString::number(level), 150, 3, QColor(255, 255, 255));
        drawTextCentered(p, "PRESS SPACE OR CLICK TO LAUNCH", 180, 1, QColor(200, 210, 230));
        break;
    case Paused:
        drawOverlay(p, "PAUSED", "", "", "PRESS P TO RESUME");
        break;
    case LevelClear:
        drawOverlay(p, "LEVEL CLEARED!",
                    QString("BONUS +") + QString::number(lastBonus),
                    QString("SCORE ") + QString::number(score),
                    "PRESS SPACE FOR THE NEXT LEVEL");
        break;
    case GameOver:
        drawOverlay(p, "GAME OVER",
                    QString("SCORE ") + QString::number(score),
                    QString("BEST ") + QString::number(highScore),
                    "PRESS SPACE TO PLAY AGAIN");
        break;
    case Won:
        drawOverlay(p, "YOU WIN!",
                    QString("SCORE ") + QString::number(score),
                    QString("BEST ") + QString::number(highScore),
                    "PRESS SPACE TO PLAY AGAIN");
        break;
    case Playing:
        break;
    }
}

void GameWidget::paintEvent(QPaintEvent *)
{
    renderFrame();

    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    // scale the small canvas up with hard edges (no smoothing)
    const int s = viewScale();
    int ox = viewOffsetX();
    int oy = viewOffsetY();
    if (shakeTimer > 0.0) {
        ox += (std::rand() % 3 - 1) * s;
        oy += (std::rand() % 3 - 1) * s;
    }
    p.drawImage(QRect(ox, oy, W * s, H * s), frame);
}
