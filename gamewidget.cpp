#include "gamewidget.h"

#include <QPainter>
#include <QBrush>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QPaintEvent>
#include <QSettings>
#include <QDate>

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

const int NUM_LEVELS = 6;                 // arcade levels (the story levels live in story.cpp)
const int MAX_LIVES  = 5;

// Development switch: every story level can be played straight away.
// Set to false for release, so levels open one by one as they are cleared.
const bool DEV_UNLOCK_ALL = true;

const double BARRIER_Y   = 234.0;         // the barrier power-up: a line under the paddle
const int    MAX_BARRIER = 3;
const int    BOSS_W      = 32;
const int    BOSS_H      = 16;
const double BOSS_Y      = 30.0;
const int    BOSS_HP     = 24;
const int    SHIELD_ROW  = 3;             // brick row Morvax rebuilds when he gets angry

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

double mouseYOf(QMouseEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().y();
#else
    return e->y();
#endif
}

int brickLeft(int c) { return BRICK_X0 + c * BRICK_W; }
int brickTop(int r)  { return BRICK_Y0 + r * BRICK_H; }

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------

// power types 8 and up are curses (power-downs), see GameWidget::PowerType
bool isCurse(int type) { return type >= 8; }

QColor powerColor(int type)
{
    switch (type) {
    case 0:  return QColor(80, 140, 255);   // wide
    case 1:  return QColor(80, 220, 220);   // slow
    case 2:  return QColor(255, 220, 80);   // multi-ball
    case 3:  return QColor(255, 120, 60);   // laser
    case 4:  return QColor(150, 230, 90);   // sticky
    case 5:  return QColor(255, 90, 30);    // fireball
    case 6:  return QColor(200, 200, 255);  // barrier
    case 7:  return QColor(220, 120, 255);  // zap
    case 8:  return QColor(255, 80, 80);    // curse: tiny paddle
    case 9:  return QColor(255, 120, 70);   // curse: fast ball
    case 10: return QColor(255, 90, 160);   // curse: reversed controls
    default: return QColor(170, 110, 255);  // curse: darkness
    }
}

char powerLetter(int type)
{
    switch (type) {
    case 0:  return 'W';
    case 1:  return 'S';
    case 2:  return 'M';
    case 3:  return 'L';
    case 4:  return 'G';
    case 5:  return 'F';
    case 6:  return 'B';
    case 7:  return 'Z';
    case 8:  return 'T';
    case 9:  return '!';
    case 10: return 'R';
    default: return 'D';
    }
}

const char *powerName(int type)
{
    static const char *const names[12] = {
        "WIDE", "SLOW", "MULTI", "LASER", "STICKY", "FIRE", "BARRIER", "ZAP",
        "TINY", "FAST", "REVERSE", "DARK"
    };
    return names[type < 0 ? 0 : (type > 11 ? 11 : type)];
}

QColor rowColor(int r)
{
    static const unsigned pal[8] = {
        0x3ec6e0, 0x4cd964, 0xd8e03e, 0xb06bff,
        0xff7ab8, 0x3e8bff, 0x7be0a0, 0x9be03e
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
    {'=', "000111000111000"},
    {'>', "100010001010100"}, {'<', "001010100010001"}, {'*', "101010111010101"},
    {',', "000000000010100"}, {'\'', "010010000000000"}, {'?', "110001010000010"},
    {'(', "010100100100010"}, {')', "010001001001010"}
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

void drawSprite(QPainter &p, const char *const *rows, int h, int x, int y, const QColor &c,
                int scale = 1)
{
    for (int r = 0; r < h; ++r)
        for (int i = 0; rows[r][i] != '\0'; ++i)
            if (rows[r][i] == '#')
                p.fillRect(x + i * scale, y + r * scale, scale, scale, c);
}

// Several-colour pixel art: every character of the sprite is looked up in a
// small palette ('.' and unknown characters are transparent).
struct PalEntry { char key; unsigned rgb; };

void drawPixelArt(QPainter &p, const char *const *rows, int h, int x, int y, int scale,
                  const PalEntry *pal, int palCount, bool allWhite = false)
{
    for (int r = 0; r < h; ++r) {
        for (int i = 0; rows[r][i] != '\0'; ++i) {
            const char ch = rows[r][i];
            for (int k = 0; k < palCount; ++k) {
                if (pal[k].key != ch) continue;
                p.fillRect(x + i * scale, y + r * scale, scale, scale,
                           allWhite ? QColor(255, 255, 255) : QColor::fromRgb(pal[k].rgb));
                break;
            }
        }
    }
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
                 const QString &l2, const QString &l3,
                 const QColor &l2color = QColor(200, 210, 230))
{
    dimScreen(p);
    drawTextCentered(p, title, 60, 3, QColor(255, 220, 90));
    drawTextCentered(p, l1, 100, 2, QColor(255, 255, 255));
    drawTextCentered(p, l2, 120, 2, l2color);
    drawTextCentered(p, l3, 150, 1, QColor(200, 210, 230));
}

// ---------------------------------------------------------------------------
// Shared sprites / drawing helpers (used by the game and by the menu screens)
// ---------------------------------------------------------------------------

const int    MENU_Y0       = 98;                       // first menu item
const int    MENU_STEP     = 15;
const int    DECOR_Y       = 64;                       // decorative bricks under the logo
const int    DECOR_ROWS    = 3;
const int    DECOR_BOTTOM  = DECOR_Y + DECOR_ROWS * BRICK_H;   // 88
const double DEMO_PADDLE_Y = 230.0;

// letters offered in the initials entry
const char NAME_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const int  NAME_COUNT   = 36;

const char *const TROPHY[9] = {
    "#########",
    ".#######.",
    ".#######.",
    ".#######.",
    "..#####..",
    "...###...",
    "....#....",
    "....#....",
    "..#####.."
};
const char *const UP_ARROW[3]   = { "..#..", ".###.", "#####" };
const char *const DOWN_ARROW[3] = { "#####", ".###.", "..#.." };

void copyText(char *dst, int cap, const QString &s)
{
    int n = 0;
    for (; n < s.length() && n < cap - 1; ++n)
        dst[n] = s.at(n).toLatin1();
    dst[n] = '\0';
}

void drawTrophy(QPainter &p, int x, int y)
{
    drawSprite(p, TROPHY, 9, x, y, QColor(255, 205, 60));
    p.fillRect(x + 2, y + 1, 1, 2, QColor(255, 245, 190));      // shine
}

// background, faint grid dots and the solid walls (top and both sides)
void drawPlayfieldFrame(QPainter &p)
{
    p.fillRect(0, 0, W, H, QColor(14, 16, 34));
    for (int ty = 3; ty < GRID_H; ++ty)
        for (int tx = 1; tx < GRID_W - 1; ++tx)
            p.fillRect(tx * TILE, ty * TILE, 1, 1, QColor(34, 38, 68));

    for (int tx = 0; tx < GRID_W; ++tx)
        drawWallTile(p, tx * TILE, HUD_H);
    for (int ty = 3; ty < GRID_H; ++ty) {
        drawWallTile(p, 0, ty * TILE);
        drawWallTile(p, (GRID_W - 1) * TILE, ty * TILE);
    }
}

// one 16 x 8 brick
void drawBrick(QPainter &p, int x, int y, const QColor &col)
{
    p.fillRect(x, y, BRICK_W, BRICK_H, col.darker(170));           // grid line / shadow
    p.fillRect(x, y, BRICK_W - 1, BRICK_H - 1, col);                // body
    p.fillRect(x, y, BRICK_W - 1, 1, col.lighter(145));             // top highlight
}

enum BrickKind { BK_NORMAL = 0, BK_SOLID, BK_HEART, BK_BOMB, BK_GHOST };

const char *const BOMB_ICON[5] = { ".###.", "#####", "#####", "#####", ".###." };

// a brick with the decoration of its kind (used by the game and the help screen)
void drawBrickCell(QPainter &p, int x, int y, const QColor &col, int kind,
                   bool cracked, bool faded, bool blink)
{
    if (kind == BK_GHOST && faded) {                 // only a faint dotted outline
        for (int i = 0; i < BRICK_W - 1; i += 2) {
            p.fillRect(x + i, y, 1, 1, col.darker(220));
            p.fillRect(x + i, y + BRICK_H - 2, 1, 1, col.darker(220));
        }
        p.fillRect(x, y + 3, 1, 1, col.darker(220));
        p.fillRect(x + BRICK_W - 2, y + 3, 1, 1, col.darker(220));
        return;
    }

    drawBrick(p, x, y, col);

    switch (kind) {
    case BK_SOLID:                                    // diagonal hatch
        for (int i = 0; i < 6; ++i) {
            p.fillRect(x + 3 + i * 2, y + 1 + (i % 2) * 2, 1, 1, QColor(0x4a, 0x50, 0x62));
            p.fillRect(x + 4 + i * 2, y + 3 + (i % 2) * 2, 1, 1, QColor(0xa8, 0xae, 0xc0));
        }
        break;
    case BK_HEART:
        drawHeart(p, x + 4, y + 1);
        break;
    case BK_BOMB:                                     // black bomb with a sparking fuse
        drawSprite(p, BOMB_ICON, 5, x + 5, y + 2, QColor(25, 15, 20));
        p.fillRect(x + 6, y + 3, 1, 1, QColor(140, 120, 130));
        p.fillRect(x + 9, y + 1, 1, 1, QColor(120, 90, 60));
        p.fillRect(x + 10, y + 1, 1, 1, blink ? QColor(255, 255, 160) : QColor(255, 140, 40));
        break;
    case BK_GHOST:                                    // two eyes and a wavy hem
        p.fillRect(x + 5, y + 2, 1, 2, QColor(70, 60, 130));
        p.fillRect(x + 9, y + 2, 1, 2, QColor(70, 60, 130));
        for (int i = 1; i < BRICK_W - 1; i += 3)
            p.fillRect(x + i, y + BRICK_H - 2, 1, 1, col.darker(140));
        break;
    default:
        break;
    }

    if (cracked) {                                    // cracks on damaged bricks
        const QColor crack(0, 0, 0, 160);
        p.fillRect(x + 4, y + 1, 1, 1, crack);
        p.fillRect(x + 5, y + 2, 1, 1, crack);
        p.fillRect(x + 6, y + 3, 1, 1, crack);
        p.fillRect(x + 7, y + 4, 1, 1, crack);
        p.fillRect(x + 8, y + 5, 1, 1, crack);
    }
}

// the paddle (also used by the menu's demo ball)
void drawPaddle(QPainter &p, double centerX, double width, int py, bool laser, bool sticky,
                bool frozen = false)
{
    const int pw = int(width + 0.5) / 2 * 2;
    const int px = int(centerX + 0.5) - pw / 2;
    const QColor body = frozen ? QColor(150, 220, 255) : QColor(80, 100, 190);
    p.fillRect(px + 1, py, pw - 2, 5, body);                           // body
    p.fillRect(px, py + 1, pw, 3, body);                               // rounded ends
    p.fillRect(px + 1, py, pw - 2, 1, frozen ? QColor(255, 255, 255) : QColor(200, 214, 255)); // highlight
    p.fillRect(px + 1, py + 4, pw - 2, 1, frozen ? QColor(90, 160, 220) : QColor(40, 50, 120)); // shadow
    if (frozen)                                                        // ice crystals
        for (int i = 3; i < pw - 2; i += 6)
            p.fillRect(px + i, py + 2, 1, 1, QColor(255, 255, 255));
    if (laser) {
        p.fillRect(px + 1, py - 3, 2, 3, QColor(255, 120, 60));
        p.fillRect(px + pw - 3, py - 3, 2, 3, QColor(255, 120, 60));
    }
    if (sticky)
        p.fillRect(px + 2, py - 1, pw - 4, 1, QColor(150, 230, 90));
}

const char *const SKULL[5] = { ".###.", "#.#.#", "#####", ".###.", ".#.#." };

// power-up capsule, 13 x 7 pixels, letter inside (x, y = top-left corner).
// Curses look different on purpose: spiky, dark red, with a skull - dodge them!
void drawCapsule(QPainter &p, int x, int y, int type, bool blink = false)
{
    if (isCurse(type)) {
        const QColor rim = blink ? QColor(255, 230, 90) : QColor(255, 60, 60);
        p.fillRect(x + 1, y, 11, 7, rim);
        p.fillRect(x, y + 1, 13, 5, rim);
        p.fillRect(x - 1, y + 3, 1, 1, rim);                  // spikes
        p.fillRect(x + 13, y + 3, 1, 1, rim);
        p.fillRect(x + 6, y - 1, 1, 1, rim);
        p.fillRect(x + 6, y + 7, 1, 1, rim);
        p.fillRect(x + 1, y + 1, 11, 5, QColor(60, 8, 18));
        drawSprite(p, SKULL, 5, x + 2, y + 1, QColor(240, 230, 230));
        drawText(p, QString(QChar(powerLetter(type))), x + 8, y + 1, 1, powerColor(type));
        return;
    }
    const QColor col = powerColor(type);
    p.fillRect(x + 1, y, 11, 7, col.lighter(150));
    p.fillRect(x, y + 1, 13, 5, col.lighter(150));
    p.fillRect(x + 1, y + 1, 11, 5, col.darker(170));
    drawText(p, QString(QChar(powerLetter(type))), x + 5, y + 1, 1, QColor(255, 255, 255));
}

// twinkling stars for the menu screens
void drawStars(QPainter &p, double t)
{
    for (int i = 0; i < 40; ++i) {
        const int sx = LEFT + 4 + (i * 137) % (RIGHT - LEFT - 8);
        const int sy = TOP + 2 + (i * 91) % (H - TOP - 6);
        const int ph = (int(t * 2.5) + i * 3) % 7;
        QColor c(52, 58, 100);
        if (ph == 0)                c = QColor(255, 255, 255);
        else if (ph == 1 || ph == 6) c = QColor(120, 130, 180);
        p.fillRect(sx, sy, 1, 1, c);
    }
}

// the big rainbow, bobbing "BRICK BREAKER" logo
void drawLogo(QPainter &p, double t, int y)
{
    const QString s("BRICK BREAKER");
    const int scale = 3;
    int x = (W - textWidth(s, scale)) / 2;
    for (int i = 0; i < s.length(); ++i, x += 4 * scale) {
        if (s.at(i).toLatin1() == ' ') continue;
        const int bob = int(std::floor(std::sin(t * 3.0 + i * 0.55) * 2.0 + 0.5));
        const QColor c = QColor::fromHsv((i * 26 + int(t * 70)) % 360, 190, 255);
        drawText(p, QString(s.at(i)), x + 1, y + bob + 2, scale, QColor(0, 0, 0));
        drawText(p, QString(s.at(i)), x, y + bob, scale, c);
    }
}


// ---------------------------------------------------------------------------
// Story mode sprites
// ---------------------------------------------------------------------------

// 12 x 12 portraits for the dialogue scenes
const char *const ELDER_ART[12] = {
    "...gggggg...",
    "..gggggggg..",
    ".gggggggggg.",
    ".ggssssssgg.",
    ".gsessssesg.",
    ".gssssssssg.",
    ".gswwsswwsg.",
    ".gwwwwwwwwg.",
    "..wwwwwwww..",
    "..gwwwwwwg..",
    ".ggwwwwwwgg.",
    "gggggwwggggg"
};
const PalEntry ELDER_PAL[4] = { {'g', 0x3f8f4f}, {'s', 0xf0c8a0}, {'e', 0x202020}, {'w', 0xf0f0f0} };

const char *const PIP_ART[12] = {
    "......y.....",
    "..y..yyy..y.",
    "...yyyyyyy..",
    "..yywwyyyyy.",
    ".yywyyyyyyyy",
    "yyyyeyyyeyyy",
    ".yyyeyyyeyy.",
    ".yyyyyyyyyy.",
    "..yyyrrryy..",
    "...yyyyyy...",
    "..y..yy..y..",
    "......y....."
};
const PalEntry PIP_PAL[4] = { {'y', 0xffe066}, {'w', 0xffffff}, {'e', 0x302010}, {'r', 0xff8040} };

const char *const MORVAX_ART[12] = {
    "h..........h",
    "hh..pppp..hh",
    ".hhpppppphh.",
    "..pppppppp..",
    ".ppddddddpp.",
    ".pdrrddrrdp.",
    ".pddddddddp.",
    ".pddmmmmddp.",
    ".ppddddddpp.",
    "pppppddppppp",
    "pppppppppppp",
    "pp.pppppp.pp"
};
const PalEntry MORVAX_PAL[5] = {
    {'h', 0xb0b0c0}, {'p', 0x6a2a9a}, {'d', 0x1a0a24}, {'r', 0xff3030}, {'m', 0xc04040}
};

// Morvax as the final boss, 32 x 16
const char *const BOSS_ART[16] = {
    "hh...........g.gg.g...........hh",
    ".hh.........gggggggg.........hh.",
    "..hh.......pggggggggp.......hh..",
    "...hhh...pppppppppppppp...hhh...",
    ".....hhpppppppppppppppppphh.....",
    "......pppddddddddddddddppp......",
    "....cpppddrrrddddddrrrddpppc....",
    "...ccpppddrrrddddddrrrddpppcc...",
    "..cccpppddddddddddddddddpppccc..",
    ".ccccpppddddmmmmmmmmddddpppcccc.",
    "cccccpppdddmdmdmdmdmmdddpppccccc",
    "cccc.ppppdddddddddddddddpppp.ccc",
    "ccc..pppppdddddddddddddppppp..cc",
    "cc....pppppppppppppppppppp....cc",
    "c......pppppp.pppp.pppppp......c",
    "........pppp...pp...pppp........"
};
const PalEntry BOSS_PAL[7] = {
    {'h', 0xb0b0c0}, {'p', 0x6a2a9a}, {'d', 0x1a0a24}, {'r', 0xff3030},
    {'m', 0xc04040}, {'g', 0xffd040}, {'c', 0x3a1460}
};

// world-map decorations, one kind per chapter (5 x 7)
const char *const TREE_ART[7] = {
    "..g..",
    ".ggg.",
    "ggggg",
    ".ggg.",
    "ggggg",
    "..t..",
    "..t.."
};
const PalEntry TREE_PAL[2] = { {'g', 0x2f8a3a}, {'t', 0x6b4a2a} };
const char *const CRYSTAL_ART[7] = {
    "..a..",
    ".aab.",
    ".aab.",
    "aaabb",
    "aaabb",
    ".aab.",
    "..b.."
};
const PalEntry CRYSTAL_PAL[2] = { {'a', 0x8fe0ff}, {'b', 0x3a8ac0} };
const char *const SPIKE_ART[7] = {
    "..d..",
    "..d..",
    ".ddd.",
    ".drd.",
    "ddddd",
    "ddrdd",
    "ddddd"
};
const PalEntry SPIKE_PAL[2] = { {'d', 0x4a2a5c}, {'r', 0xff5030} };

const char *const LOCK[6]   = { ".###.", "#...#", "#####", "##.##", "##.##", "#####" };
const char *const STAR7[7]  = { "...#...", "..###..", "#######", ".#####.", "..###..", ".##.##.", ".#...#." };
const char *const STAR3[3]  = { ".#.", "###", ".#." };
const char *const SHARD[6]  = { "..#..", ".###.", "#####", "#####", ".###.", "..#.." };

void drawPortrait(QPainter &p, int speaker, int x, int y, int scale)
{
    switch (speaker) {
    case Story::ELDER:  drawPixelArt(p, ELDER_ART, 12, x, y, scale, ELDER_PAL, 4);   break;
    case Story::PIP:    drawPixelArt(p, PIP_ART, 12, x, y, scale, PIP_PAL, 4);       break;
    case Story::MORVAX: drawPixelArt(p, MORVAX_ART, 12, x, y, scale, MORVAX_PAL, 5); break;
    default: break;
    }
}

QColor chapterColor(int c)
{
    return QColor::fromRgb(Story::chapter(c).color);
}

QString romanNumeral(int n)
{
    static const char *const r[4] = { "I", "II", "III", "IV" };
    return QString(r[n < 0 ? 0 : (n > 3 ? 3 : n)]);
}

// Splits text into lines of at most maxChars characters, breaking at spaces.
int wrapText(const QString &text, int maxChars, QString out[], int maxLines)
{
    int n = 0;
    QString line;
    int i = 0;
    const int len = text.length();
    while (i < len && n < maxLines) {
        int j = i;
        while (j < len && text.at(j) != QChar(' ')) ++j;
        const QString word = text.mid(i, j - i);
        if (line.isEmpty()) {
            line = word;
        } else if (line.length() + 1 + word.length() <= maxChars) {
            line += QChar(' ');
            line += word;
        } else {
            out[n++] = line;
            line = word;
        }
        i = j + 1;
    }
    if (!line.isEmpty() && n < maxLines) out[n++] = line;
    return n;
}

// ---------------------------------------------------------------------------
// World map geometry. The map is a tall picture (MAP_H pixels) that scrolls
// inside the viewport; the levels sit on a winding path from bottom to top.
// ---------------------------------------------------------------------------

const int MAP_STEP    = 34;                  // vertical distance between two levels
const int CHAPTER_GAP = 20;                  // extra space where a new chapter starts
const int MAP_VIEW_Y  = HUD_H;               // viewport on screen
const int MAP_VIEW_H  = 184;
const int MAP_PANEL_Y = MAP_VIEW_Y + MAP_VIEW_H;   // info panel below the map
const int MAP_H       = 40 + (Story::LEVELS - 1) * MAP_STEP
                        + (Story::CHAPTERS - 1) * CHAPTER_GAP + 72;

int mapNodeX(int n) { return W / 2 + int(std::sin((n - 1) * 0.95) * 58.0); }
int mapNodeY(int n) { return MAP_H - 40 - (n - 1) * MAP_STEP - Story::chapterOf(n) * CHAPTER_GAP; }

// map y where chapter c (1 or 2) begins
int chapterBorderY(int c)
{
    const int first = c * Story::LEVELS_PER_CHAPTER + 1;
    return (mapNodeY(first) + mapNodeY(first - 1)) / 2;
}

int chapterAtMapY(int wy)
{
    for (int c = Story::CHAPTERS - 1; c > 0; --c)
        if (wy < chapterBorderY(c)) return c;
    return 0;
}

// x of the path at map height wy (straight lines between the level nodes)
double mapPathX(double wy)
{
    if (wy >= mapNodeY(1)) return mapNodeX(1);
    for (int n = 1; n < Story::LEVELS; ++n) {
        const double y0 = mapNodeY(n), y1 = mapNodeY(n + 1);
        if (wy <= y0 && wy >= y1) {
            const double t = (y0 - wy) / (y0 - y1);
            return mapNodeX(n) + (mapNodeX(n + 1) - mapNodeX(n)) * t;
        }
    }
    return mapNodeX(Story::LEVELS);
}

double mapCamTarget(int sel)
{
    return clampd(mapNodeY(sel) - MAP_VIEW_H * 0.55, 0.0, double(MAP_H - MAP_VIEW_H));
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

    loadScores();                       // the saved top-10 table (local storage)
    loadStoryProgress();                // stars and best scores of the story levels
    messageColor = QColor(255, 240, 150);

    mbx = 70.0; mby = 150.0; mvx = 52.0; mvy = -68.0; demoPadX = W / 2.0;
    for (int i = 0; i < TRAIL; ++i) { trailX[i] = mbx; trailY[i] = mby; }
    for (int i = 0; i < COLS; ++i)  flash[i] = 0.0;

    for (int i = 0; i < MAX_BALLS; ++i)     balls[i].active = false;
    for (int i = 0; i < MAX_POWERUPS; ++i)  powerups[i].active = false;
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].active = false;
    for (int i = 0; i < MAX_LASERS; ++i)    lasers[i].active = false;
    for (int i = 0; i < MAX_BOLTS; ++i)     bolts[i].active = false;
    for (int i = 0; i < MAX_POPUPS; ++i)    popups[i].active = false;
    for (int i = 0; i < MAX_ZAPS; ++i)      zaps[i].active = false;

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
    sound.play(SoundManager::SFX_START);
    storyMode = false;
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
    const char *const *layout = storyMode ? Story::level(lvl).rows
                                          : LEVELS[(lvl - 1) % NUM_LEVELS];
    bricksLeft = 0;
    bricksBroken = 0;

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            const char ch = layout[r][c];
            Brick &b = bricks[r][c];
            b.alive = false;
            b.solid = false;
            b.heart = false;
            b.bomb = false;
            b.ghost = false;
            b.hp = 0;
            b.maxHp = 0;
            if (ch >= '1' && ch <= '4') {
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
            } else if (ch == 'B' || ch == 'G') {
                b.alive = true;
                b.bomb  = (ch == 'B');
                b.ghost = (ch == 'G');
                b.hp = b.maxHp = 1;
                ++bricksLeft;
            }
        }
    }
    for (int i = 0; i < MAX_POWERUPS; ++i) powerups[i].active = false;
    for (int i = 0; i < MAX_LASERS; ++i)   lasers[i].active = false;
    for (int i = 0; i < MAX_BOLTS; ++i)    bolts[i].active = false;
    for (int i = 0; i < MAX_ZAPS; ++i)     zaps[i].active = false;
    for (int i = 0; i < MAX_POPUPS; ++i)   popups[i].active = false;

    // the final quest: Morvax himself
    boss.active = storyMode && Story::level(lvl).boss;
    boss.dying = false;
    boss.x = W / 2.0 - BOSS_W / 2.0;
    boss.y = BOSS_Y;
    boss.vx = 28.0;
    boss.hp = boss.maxHp = BOSS_HP;
    boss.phase = 0;
    boss.hitFlash = 0.0;
    boss.attackTimer = 3.0;
    boss.dyingTimer = 0.0;
}

void GameWidget::startNextLevel()
{
    sound.play(SoundManager::SFX_START);
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
    for (int i = 0; i < MAX_BOLTS; ++i)    bolts[i].active = false;

    if (storyMode) {
        // stars: 3 for a flawless clear, 2 if one life was lost, otherwise 1
        starsEarned = livesLost == 0 ? 3 : (livesLost == 1 ? 2 : 1);
        starsShown = 0;
        clearTimer = 0.0;
        newBest = score > storyBest[level];
        saveStoryResult(level, starsEarned, score);
        state = LevelClear;
        endLock = 0.6;
        sound.play(SoundManager::SFX_LEVEL_CLEAR);
        return;
    }

    if (level >= NUM_LEVELS) {
        state = Won;
        endLock = 0.8;
        sound.play(SoundManager::SFX_WIN);
    } else {
        state = LevelClear;
        sound.play(SoundManager::SFX_LEVEL_CLEAR);
    }
}

void GameWidget::loseLife()
{
    --lives;
    ++livesLost;
    combo = 0;
    shakeTimer = 0.3;
    resetEffects();
    for (int i = 0; i < MAX_POWERUPS; ++i) powerups[i].active = false;
    for (int i = 0; i < MAX_LASERS; ++i)   lasers[i].active = false;
    for (int i = 0; i < MAX_BOLTS; ++i)    bolts[i].active = false;

    if (lives <= 0) {
        state = GameOver;
        endLock = 0.8;
        sound.play(SoundManager::SFX_GAME_OVER);
    } else {
        resetBallOnPaddle();
        state = Ready;
        sound.play(SoundManager::SFX_LIFE_LOST);
    }
}

void GameWidget::grantLife()
{
    sound.play(SoundManager::SFX_EXTRA_LIFE);
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
    fireTimer = 0.0;
    tinyTimer = 0.0;
    fastTimer = 0.0;
    reverseTimer = 0.0;
    darkTimer = 0.0;
    freezeTimer = 0.0;
    barrier = 0;
    laserCooldown = 0.0;
}

void GameWidget::primaryAction()
{
    switch (state) {
    case Menu:
        activateMenu(menuSel);
        break;
    case HowTo:
    case Scores:
        goMenu();
        break;
    case EnterName:
        break;
    case WorldMap:
        playSelected();
        break;
    case Scene:
        advanceScene();
        break;
    case Ending:
        if (endLock <= 0.0) openMap(Story::LEVELS, false);
        break;
    case GameOver:
    case Won:
        if (endLock > 0.0) break;                 // ignore button-mashing for a moment
        if (storyMode)        startStoryLevel(level, false);   // retry, without the story again
        else if (qualifies(score)) beginNameEntry();   // made the table: enter initials first
        else                  newGame();
        break;
    case LevelClear:
        if (storyMode) {
            if (endLock <= 0.0) continueStory();
        } else {
            startNextLevel();
        }
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
    if (state == Playing) {
        state = Paused;
        sound.play(SoundManager::SFX_PAUSE);
    } else if (state == Paused) {
        state = Playing;
        sound.play(SoundManager::SFX_RESUME);
    }
}

// ---------------------------------------------------------------------------
// High scores: a top-10 table kept in a local SQLite database (scoreDb).
// scores[] is an in-memory copy of the best rows, so drawing never touches
// the database.
// ---------------------------------------------------------------------------

void GameWidget::loadScores()
{
    // one-time import of scores saved by older versions (QSettings)
    QSettings st;
    if (scoreDb.isOpen() && scoreDb.isEmpty()) {
        bool found = false;
        bool allOk = true;
        for (int i = 0; i < MAX_SCORES; ++i) {
            const QString k = QString("scores/%1/").arg(i);
            if (!st.contains(k + "score")) break;
            found = true;
            allOk &= scoreDb.insert(st.value(k + "name", QString("---")).toString(),
                                    st.value(k + "score", 0).toInt(),
                                    st.value(k + "level", 1).toInt(),
                                    st.value(k + "date", QString()).toString());
        }
        const int old = st.value("highScore", 0).toInt();
        if (!found && old > 0)
            allOk &= scoreDb.insert("OLD", old, 1, QString());
        if (allOk) {                    // only forget the old data once it is safely in the db
            st.remove("scores");
            st.remove("highScore");
        }
    }

    Leaderboard::Row rows[MAX_SCORES];
    scoreCount = scoreDb.load(rows, MAX_SCORES);
    for (int i = 0; i < scoreCount; ++i) {
        ScoreEntry &e = scores[i];
        copyText(e.name, 4, rows[i].name);
        copyText(e.date, 9, rows[i].date);
        e.score = rows[i].score < 0 ? 0 : rows[i].score;
        e.level = rows[i].level;
    }
    highScore = scoreCount > 0 ? scores[0].score : 0;
}

// does this score earn a place in the table?
bool GameWidget::qualifies(int s) const
{
    return s > 0 && (scoreCount < MAX_SCORES || s > scores[MAX_SCORES - 1].score);
}

// row (0 = best) this score would take; equal scores go below the older ones
int GameWidget::scoreRank(int s) const
{
    int r = 0;
    while (r < scoreCount && scores[r].score >= s) ++r;
    return r;
}

int GameWidget::insertScore(const char *name, int s, int lvl)
{
    const int pos = scoreRank(s);
    if (pos >= MAX_SCORES) return -1;

    const int last = scoreCount < MAX_SCORES ? scoreCount : MAX_SCORES - 1;
    for (int i = last; i > pos; --i)
        scores[i] = scores[i - 1];

    ScoreEntry &e = scores[pos];
    copyText(e.name, 4, QString(name));
    e.score = s;
    e.level = lvl;
    copyText(e.date, 9, QDate::currentDate().toString("dd/MM/yy"));

    if (scoreCount < MAX_SCORES) ++scoreCount;
    highScore = scores[0].score;
    scoreDb.insert(QString(e.name), e.score, e.level, QString(e.date));
    return pos;
}

void GameWidget::clearScores()
{
    scoreCount = 0;
    highScore = 0;
    scoreDb.clear();
}

void GameWidget::beginNameEntry()
{
    // start from the initials used last time
    QSettings st;
    const QString last = st.value("lastName", QString("AAA")).toString();
    for (int i = 0; i < 3; ++i) {
        nameIdx[i] = 0;
        if (i < last.length()) {
            const char ch = char(std::toupper((unsigned char)last.at(i).toLatin1()));
            for (int j = 0; j < NAME_COUNT; ++j)
                if (NAME_CHARS[j] == ch) nameIdx[i] = j;
        }
    }
    nameCursor = 0;
    confettiTimer = 0.0;
    state = EnterName;
    sound.play(SoundManager::SFX_EXTRA_LIFE);
}

void GameWidget::confirmName()
{
    char name[4];
    for (int i = 0; i < 3; ++i) name[i] = NAME_CHARS[nameIdx[i]];
    name[3] = '\0';

    QSettings st;
    st.setValue("lastName", QString(name));

    scoreHighlight = insertScore(name, score, level);
    clearArmed = false;
    state = Scores;
    sound.play(SoundManager::SFX_LEVEL_CLEAR);
    for (int i = 0; i < 6; ++i)
        spawnParticles(LEFT + 12 + i * 30, 40, QColor::fromHsv(i * 55, 200, 255), 10);
}

// ---------------------------------------------------------------------------
// Menu flow
// ---------------------------------------------------------------------------

bool GameWidget::isMenuState() const
{
    return state == Menu || state == HowTo || state == Scores || state == EnterName;
}

void GameWidget::goMenu(int sel)
{
    if (sel >= 0) menuSel = sel;
    state = Menu;
    clearArmed = false;
    scoreHighlight = -1;
}

void GameWidget::finishRun()
{
    if (storyMode)        { openMap(level, false); return; }
    if (qualifies(score)) beginNameEntry();
    else                  goMenu(0);
}

QString GameWidget::menuLabel(int item) const
{
    switch (item) {
    case MI_STORY:  return QString("STORY MODE");
    case MI_ARCADE: return QString("ARCADE");
    case MI_SCORES: return QString("HIGH SCORES");
    case MI_HOWTO:  return QString("HOW TO PLAY");
    case MI_SOUND:  return sound.isMuted() ? QString("SOUND: OFF") : QString("SOUND: ON");
    default:        return QString("QUIT");
    }
}

void GameWidget::activateMenu(int item)
{
    switch (item) {
    case MI_STORY: {
        // start on the first level that has no stars yet
        int sel = 1;
        while (sel < unlockedCount() && storyStars[sel] > 0) ++sel;
        sound.play(SoundManager::SFX_RESUME);
        openMap(sel, true);
        break;
    }
    case MI_ARCADE:
        newGame();
        break;
    case MI_SCORES:
        scoreHighlight = -1;
        clearArmed = false;
        state = Scores;
        sound.play(SoundManager::SFX_RESUME);
        break;
    case MI_HOWTO:
        howToPage = 0;
        state = HowTo;
        sound.play(SoundManager::SFX_RESUME);
        break;
    case MI_SOUND:
        sound.toggleMuted();
        if (!sound.isMuted()) sound.play(SoundManager::SFX_START);
        break;
    default:
        close();
        break;
    }
}

// which menu item is under this canvas position (-1 = none)
int GameWidget::menuItemAt(double lx, double ly) const
{
    for (int i = 0; i < MENU_ITEMS; ++i) {
        const int y = MENU_Y0 + i * MENU_STEP;
        if (ly >= y - 3 && ly < y + 13 && lx >= 36 && lx < W - 36)
            return i;
    }
    return -1;
}

// the little ball that bounces behind the menu (a self-playing demo)
void GameWidget::updateMenuDemo(double dt)
{
    trailTimer += dt;
    if (trailTimer >= 0.035) {
        trailTimer = 0.0;
        for (int i = TRAIL - 1; i > 0; --i) {
            trailX[i] = trailX[i - 1];
            trailY[i] = trailY[i - 1];
        }
        trailX[0] = mbx;
        trailY[0] = mby;
    }
    for (int c = 0; c < COLS; ++c)
        if (flash[c] > 0.0) flash[c] -= dt;

    mbx += mvx * dt;
    mby += mvy * dt;

    if (mbx < LEFT + BALL_R)       { mbx = LEFT + BALL_R;  mvx =  std::fabs(mvx); }
    else if (mbx > RIGHT - BALL_R) { mbx = RIGHT - BALL_R; mvx = -std::fabs(mvx); }

    // the decorative bricks act as the ceiling
    if (mvy < 0.0 && mby - BALL_R < DECOR_BOTTOM) {
        mby = DECOR_BOTTOM + BALL_R;
        mvy = std::fabs(mvy);
        int c = int((mbx - LEFT) / BRICK_W);
        if (c < 0) c = 0;
        if (c >= COLS) c = COLS - 1;
        flash[c] = 0.3;
        spawnParticles(mbx, DECOR_BOTTOM - 2, rowColor(4), 6);
    }

    // an invisible helper paddle follows the ball
    const double target = mbx + std::sin(animTime * 1.7) * 9.0;
    demoPadX += (target - demoPadX) * clampd(dt * 8.0, 0.0, 1.0);
    demoPadX = clampd(demoPadX, LEFT + 16.0, RIGHT - 16.0);
    if (mvy > 0.0 && mby + BALL_R >= DEMO_PADDLE_Y && mby < DEMO_PADDLE_Y + 4.0) {
        mby = DEMO_PADDLE_Y - BALL_R;
        mvy = -std::fabs(mvy);
        mvx += (rnd() - 0.5) * 40.0;
        if (std::fabs(mvx) < 28.0) mvx = (mvx < 0.0 ? -28.0 : 28.0);
        if (std::fabs(mvx) > 75.0) mvx = (mvx < 0.0 ? -75.0 : 75.0);
    }
    if (mby > H) {                                    // safety net
        mby = DEMO_PADDLE_Y - 20.0;
        mvy = -std::fabs(mvy);
    }
}

void GameWidget::showMessage(const QString &text, double seconds, const QColor &color)
{
    message = text;
    messageTimer = seconds;
    messageColor = color;
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void GameWidget::tick()
{
    double dt = clock.restart() / 1000.0;
    if (dt > 0.04) dt = 0.04;

    animTime += dt;
    if (endLock > 0.0) endLock -= dt;
    if (state == Menu) updateMenuDemo(dt);
    if (state == EnterName) {                         // confetti while typing initials
        confettiTimer -= dt;
        if (confettiTimer <= 0.0) {
            confettiTimer = 0.07;
            spawnParticles(LEFT + 4 + rnd() * (RIGHT - LEFT - 8), TOP + 2,
                           QColor::fromHsv(int(rnd() * 360), 200, 255), 2);
        }
    }

    if (state == WorldMap)                            // the map camera glides to the selection
        mapCam += (mapCamTarget(mapSel) - mapCam) * clampd(dt * 6.0, 0.0, 1.0);

    if (state == Scene) {                             // typewriter text
        const int before = int(sceneChars);
        const int len = scenePageLength();
        sceneChars = std::fmin(double(len), sceneChars + dt * 45.0);
        if (int(sceneChars) / 3 != before / 3 && int(sceneChars) < len)
            sound.play(SoundManager::SFX_TYPE);
    }

    if (state == Ending) {                            // fireworks over the restored kingdom
        confettiTimer -= dt;
        if (confettiTimer <= 0.0) {
            confettiTimer = 0.35;
            spawnParticles(LEFT + 20 + rnd() * (RIGHT - LEFT - 40), TOP + 20 + rnd() * 90,
                           QColor::fromHsv(int(rnd() * 360), 200, 255), 24);
        }
    }

    if (state == LevelClear && storyMode) {           // the stars pop in one by one
        clearTimer += dt;
        while (starsShown < starsEarned && clearTimer >= 0.5 + starsShown * 0.4) {
            spawnParticles(W / 2 - 24 + starsShown * 24, 92, QColor(255, 215, 60), 16);
            ++starsShown;
            sound.play(SoundManager::SFX_STAR);
        }
    }

    if (state == Ready || state == Playing) {
        double move = 0.0;
        if (leftHeld)  move -= 1.0;
        if (rightHeld) move += 1.0;
        if (reverseTimer > 0.0) move = -move;         // curse: controls are flipped
        if (freezeTimer <= 0.0)                       // a frozen paddle cannot move
            paddleX += move * PADDLE_SPEED * dt;

        double target = basePaddleW();
        if (wideTimer > 0.0) target += PADDLE_W_WIDE - PADDLE_W_BASE;
        if (tinyTimer > 0.0) target -= 12.0;
        paddleW += (target - paddleW) * clampd(dt * 10.0, 0.0, 1.0);
        paddleX = clampd(paddleX, LEFT + paddleW / 2.0, RIGHT - paddleW / 2.0);
    }

    if (state == Playing) {
        if (wideTimer > 0.0)     wideTimer = std::fmax(0.0, wideTimer - dt);
        if (slowTimer > 0.0)     slowTimer = std::fmax(0.0, slowTimer - dt);
        if (laserTimer > 0.0)    laserTimer = std::fmax(0.0, laserTimer - dt);
        if (stickyTimer > 0.0)   stickyTimer = std::fmax(0.0, stickyTimer - dt);
        if (fireTimer > 0.0)     fireTimer = std::fmax(0.0, fireTimer - dt);
        if (tinyTimer > 0.0)     tinyTimer = std::fmax(0.0, tinyTimer - dt);
        if (fastTimer > 0.0)     fastTimer = std::fmax(0.0, fastTimer - dt);
        if (reverseTimer > 0.0)  reverseTimer = std::fmax(0.0, reverseTimer - dt);
        if (darkTimer > 0.0)     darkTimer = std::fmax(0.0, darkTimer - dt);
        if (freezeTimer > 0.0)   freezeTimer = std::fmax(0.0, freezeTimer - dt);
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
    if (state == Playing) updateBolts(dt);
    if (state == Playing) updateBoss(dt);

    if (state != Paused)
        updateParticles(dt);

    if (shakeTimer > 0.0)   shakeTimer = std::fmax(0.0, shakeTimer - dt);
    if (flashTimer > 0.0)   flashTimer = std::fmax(0.0, flashTimer - dt);
    if (boss.hitFlash > 0.0) boss.hitFlash = std::fmax(0.0, boss.hitFlash - dt);
    if (messageTimer > 0.0) messageTimer = std::fmax(0.0, messageTimer - dt);

    update();
}

double GameWidget::currentSpeed() const
{
    // story levels set their own speed; arcade speeds up level by level
    const double base = storyMode ? Story::level(level).speed : 92.0 + (level - 1) * 8.0;
    const double cap  = storyMode ? base * 0.25 : 35.0;
    double boost = bricksBroken * 0.5;
    if (boost > cap) boost = cap;
    double sp = base + boost;                         // pixels per second
    if (slowTimer > 0.0) sp *= 0.7;
    if (fastTimer > 0.0) sp *= 1.35;
    return sp;
}

double GameWidget::basePaddleW() const
{
    return storyMode ? Story::level(level).paddle : PADDLE_W_BASE;
}

bool GameWidget::isBossLevel() const
{
    return storyMode && Story::level(level).boss;
}

// Ghost bricks are solid for 1.6 s, then faded for 0.9 s. The wave runs
// across the wall, so there is always a gap opening somewhere.
bool GameWidget::ghostSolid(int r, int c) const
{
    if (!bricks[r][c].ghost) return true;
    return std::fmod(animTime + c * 0.18 + r * 0.33, 2.5) < 1.6;
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
            if (b.x < LEFT + BALL_R)       { b.x = LEFT + BALL_R;   b.vx =  std::fabs(b.vx); sound.play(SoundManager::SFX_WALL); }
            else if (b.x > RIGHT - BALL_R) { b.x = RIGHT - BALL_R;  b.vx = -std::fabs(b.vx); sound.play(SoundManager::SFX_WALL); }
            if (b.y < TOP + BALL_R)        { b.y = TOP + BALL_R;    b.vy =  std::fabs(b.vy); sound.play(SoundManager::SFX_WALL); }

            // the barrier power-up catches the ball once per charge
            if (barrier > 0 && b.vy > 0.0 && b.y + BALL_R >= BARRIER_Y) {
                b.y = BARRIER_Y - BALL_R;
                b.vy = -std::fabs(b.vy);
                --barrier;
                spawnParticles(b.x, BARRIER_Y, powerColor(PU_BARRIER), 10);
                sound.play(SoundManager::SFX_SOLID);
            }

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
                sound.play(SoundManager::SFX_PADDLE);
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
                    if (!bricks[r][c].alive || !ghostSolid(r, c)) continue;
                    const double left = brickLeft(c);
                    const double top  = brickTop(r);
                    const double nx = clampd(b.x, left, left + BRICK_W);
                    const double ny = clampd(b.y, top,  top + BRICK_H);
                    const double dx = b.x - nx;
                    const double dy = b.y - ny;
                    const double d2 = dx * dx + dy * dy;
                    if (d2 < BALL_R * BALL_R && fireTimer > 0.0 && !bricks[r][c].solid) {
                        hitBrick(r, c, 99);           // a fireball burns straight through
                        hit = true;
                    } else if (d2 < BALL_R * BALL_R) {
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

            // Morvax: bounces like a big brick and takes damage
            if (boss.active && !boss.dying) {
                const double nx = clampd(b.x, boss.x, boss.x + BOSS_W);
                const double ny = clampd(b.y, boss.y, boss.y + BOSS_H);
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
                        b.vy = std::fabs(b.vy);
                    }
                    bossHit(fireTimer > 0.0 ? 2 : 1);
                    if (!b.active) break;             // the killing blow clears the balls
                }
            }
        }

        if (b.active && fireTimer > 0.0 && rnd() < 0.6)   // flame trail
            spawnParticles(b.x, b.y, QColor::fromHsv(int(rnd() * 40), 230, 255), 1);
    }

    if (state == Playing && activeBallCount() == 0 && !boss.dying)
        loseLife();
}

void GameWidget::hitBrick(int r, int c, int damage)
{
    Brick &br = bricks[r][c];
    const double cx = brickLeft(c) + BRICK_W / 2.0;
    const double cy = brickTop(r) + BRICK_H / 2.0;

    if (br.solid) {                                   // solid brick: just a few sparks
        spawnParticles(cx, cy, QColor(0x9a, 0xa0, 0xb4), 3);
        sound.play(SoundManager::SFX_SOLID);
        return;
    }

    if (br.heart) {                                   // the extra-life brick
        br.hp -= damage;
        if (br.hp <= 0) {
            br.alive = false;
            score += 100;
            spawnPopup(cx, cy, 100);
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
        const int points = 10 * br.maxHp * mult;
        score += points;
        spawnPopup(cx, cy - 4, points);
        sound.play(SoundManager::SFX_BRICK_BREAK_1 + (mult - 1));   // higher combo = higher note
        spawnParticles(cx, cy, col, 14);
        const int dropPct = storyMode ? Story::level(level).dropPct : 18;
        if (std::rand() % 100 < dropPct)
            spawnPowerUp(cx, cy, rollPowerType());
        if (br.bomb)
            explodeBrick(r, c);
    } else {
        score += 5 * mult;
        sound.play(SoundManager::SFX_BRICK_HIT);
        spawnParticles(cx, cy, col, 4);
    }

    if (score > highScore) highScore = score;
    // (a bomb chain can call this many times: only the first clear counts;
    //  the boss level is won by beating Morvax, not by clearing bricks)
    if (bricksLeft <= 0 && state == Playing && !isBossLevel()) levelComplete();
}

// A bomb brick destroys every breakable brick around it (chains into other bombs)
void GameWidget::explodeBrick(int r, int c)
{
    const double cx = brickLeft(c) + BRICK_W / 2.0;
    const double cy = brickTop(r) + BRICK_H / 2.0;
    spawnParticles(cx, cy, QColor(255, 170, 40), 24);
    spawnParticles(cx, cy, QColor(255, 80, 30), 16);
    sound.play(SoundManager::SFX_EXPLODE);
    shakeTimer = std::fmax(shakeTimer, 0.25);
    flashTimer = 0.08;

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            const int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
            if (!bricks[nr][nc].alive || bricks[nr][nc].solid) continue;
            hitBrick(nr, nc, 99);
        }
    }
}

void GameWidget::launchBalls()
{
    const double spd = currentSpeed();
    bool released = false;
    for (int i = 0; i < MAX_BALLS; ++i) {
        Ball &b = balls[i];
        if (!b.active || !b.stuck) continue;
        released = true;
        b.stuck = false;
        double a = clampd(b.stuckOffset / (paddleW / 2.0), -1.0, 1.0) * 0.9;
        if (std::fabs(a) < 0.1) a = (rnd() - 0.5) * 0.5;
        b.vx = spd * std::sin(a);
        b.vy = -spd * std::cos(a);
    }
    if (released)
        sound.play(SoundManager::SFX_LAUNCH);
}

void GameWidget::fireLasers()
{
    if (laserTimer <= 0.0 || laserCooldown > 0.0) return;
    spawnLaser(paddleX - paddleW / 2.0 + 3.0);
    spawnLaser(paddleX + paddleW / 2.0 - 4.0);
    laserCooldown = 0.25;
    sound.play(SoundManager::SFX_LASER);
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

// What a broken brick drops: mostly power-ups, but later levels drop more curses
int GameWidget::rollPowerType() const
{
    const int cursePct = storyMode ? Story::level(level).cursePct : 20;
    if (std::rand() % 100 < cursePct) {
        // darkness only shows up from chapter II on
        const int kinds = (storyMode && Story::chapterOf(level) == 0) ? 3 : 4;
        return PD_TINY + std::rand() % kinds;
    }
    const int roll = std::rand() % 100;
    if (roll < 18) return PU_WIDE;
    if (roll < 32) return PU_SLOW;
    if (roll < 50) return PU_MULTI;
    if (roll < 62) return PU_LASER;
    if (roll < 72) return PU_STICKY;
    if (roll < 82) return PU_FIRE;
    if (roll < 92) return PU_BARRIER;
    return PU_ZAP;
}

void GameWidget::spawnPowerUp(double x, double y, int type)
{
    for (int i = 0; i < MAX_POWERUPS; ++i) {
        if (powerups[i].active) continue;
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
        tinyTimer = 0.0;                              // cancels the tiny-paddle curse
        showMessage("WIDE PADDLE", 1.5);
        break;
    case PU_SLOW:
        slowTimer = 10.0;
        fastTimer = 0.0;                              // cancels the fast-ball curse
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
    case PU_FIRE:
        fireTimer = 8.0;
        showMessage("FIREBALL!", 1.5, powerColor(PU_FIRE).lighter(130));
        break;
    case PU_BARRIER:
        if (barrier < MAX_BARRIER) ++barrier;
        showMessage("BARRIER", 1.5, powerColor(PU_BARRIER));
        break;
    case PU_ZAP:
        zapBricks();
        showMessage("ZAP!", 1.2, powerColor(PU_ZAP));
        break;
    case PD_TINY:
        tinyTimer = 10.0;
        wideTimer = 0.0;
        showMessage("CURSE: TINY PADDLE", 1.8, QColor(255, 90, 90));
        break;
    case PD_FAST:
        fastTimer = 8.0;
        slowTimer = 0.0;
        showMessage("CURSE: FAST BALL", 1.8, QColor(255, 90, 90));
        break;
    case PD_REVERSE:
        reverseTimer = 7.0;
        showMessage("CURSE: REVERSED!", 1.8, QColor(255, 90, 90));
        break;
    case PD_DARK:
        darkTimer = 7.0;
        showMessage("CURSE: DARKNESS", 1.8, QColor(255, 90, 90));
        break;
    default:
        break;
    }
}

// ZAP: lightning strikes four random bricks (and Morvax, if he is here)
void GameWidget::zapBricks()
{
    int cand[ROWS * COLS];
    int n = 0;
    for (int r = 0; r < ROWS; ++r)
        for (int c = 0; c < COLS; ++c)
            if (bricks[r][c].alive && !bricks[r][c].solid && !bricks[r][c].heart)
                cand[n++] = r * COLS + c;

    sound.play(SoundManager::SFX_ZAP);
    flashTimer = 0.1;
    for (int k = 0; k < 4 && n > 0; ++k) {
        const int pick = std::rand() % n;
        const int id = cand[pick];
        cand[pick] = cand[--n];
        const int r = id / COLS, c = id % COLS;
        if (!bricks[r][c].alive || state != Playing) continue;   // already gone in a blast
        spawnZap(brickLeft(c) + BRICK_W / 2.0, brickTop(r) + BRICK_H / 2.0);
        hitBrick(r, c, 99);
    }
    if (boss.active && !boss.dying && state == Playing) {
        spawnZap(boss.x + BOSS_W / 2.0, boss.y + BOSS_H / 2.0);
        bossHit(2);
    }
}

void GameWidget::spawnZap(double x, double y)
{
    for (int i = 0; i < MAX_ZAPS; ++i) {
        if (zaps[i].active) continue;
        zaps[i].active = true;
        zaps[i].x = x;
        zaps[i].y = y;
        zaps[i].life = 0.3;
        spawnParticles(x, y, powerColor(PU_ZAP), 8);
        return;
    }
}

void GameWidget::spawnPopup(double x, double y, int value)
{
    for (int i = 0; i < MAX_POPUPS; ++i) {
        if (popups[i].active) continue;
        popups[i].active = true;
        popups[i].x = x;
        popups[i].y = y;
        popups[i].life = 0.8;
        popups[i].value = value;
        return;
    }
}

// ---------------------------------------------------------------------------
// The final boss: Morvax drifts along the top, throws shadow bolts (they
// freeze the paddle) and curses, and rebuilds his shield as he gets angry.
// ---------------------------------------------------------------------------

void GameWidget::updateBoss(double dt)
{
    if (!boss.active) return;

    if (boss.dying) {                                 // a chain of explosions, then victory
        boss.dyingTimer -= dt;
        if (rnd() < 0.5)
            spawnParticles(boss.x + rnd() * BOSS_W, boss.y + rnd() * BOSS_H,
                           QColor::fromHsv(int(rnd() * 60), 220, 255), 6);
        if (rnd() < 0.08) sound.play(SoundManager::SFX_EXPLODE);
        shakeTimer = std::fmax(shakeTimer, 0.05);
        if (boss.dyingTimer <= 0.0) {
            boss.active = false;
            spawnParticles(boss.x + BOSS_W / 2.0, boss.y + BOSS_H / 2.0, QColor(200, 120, 255), 50);
            spawnParticles(boss.x + BOSS_W / 2.0, boss.y + BOSS_H / 2.0, QColor(255, 240, 150), 40);
            flashTimer = 0.3;
            levelComplete();
        }
        return;
    }

    const double speed = 28.0 + boss.phase * 14.0;
    boss.vx = (boss.vx < 0.0 ? -speed : speed);
    boss.x += boss.vx * dt;
    if (boss.x < LEFT + 2)               { boss.x = LEFT + 2;               boss.vx =  speed; }
    else if (boss.x > RIGHT - 2 - BOSS_W) { boss.x = RIGHT - 2 - BOSS_W; boss.vx = -speed; }

    boss.attackTimer -= dt;
    if (boss.attackTimer <= 0.0) {
        boss.attackTimer = 2.4 - boss.phase * 0.55;
        const double bx = boss.x + BOSS_W / 2.0;
        const double by = boss.y + BOSS_H;
        // aimed at the paddle; when furious he fires a spread of three
        const double dx = paddleX - bx;
        const double dy = PADDLE_Y - by;
        const double len = std::sqrt(dx * dx + dy * dy);
        const double v = 70.0 + boss.phase * 12.0;
        const double ax = dx / len, ay = dy / len;
        spawnBolt(bx, by, ax * v, ay * v);
        if (boss.phase >= 2) {
            for (int k = -1; k <= 1; k += 2) {
                const double a = 0.35 * k;
                spawnBolt(bx, by, (ax * std::cos(a) - ay * std::sin(a)) * v,
                                  (ax * std::sin(a) + ay * std::cos(a)) * v);
            }
        }
        sound.play(SoundManager::SFX_BOSS_SHOOT);
        if (rnd() < 0.2 + 0.1 * boss.phase)           // now and then, a curse
            spawnPowerUp(bx, by, PD_TINY + std::rand() % 4);
    }
}

void GameWidget::bossHit(int damage)
{
    if (!boss.active || boss.dying) return;
    boss.hp -= damage;
    boss.hitFlash = 0.15;
    score += 50 * damage;
    if (score > highScore) highScore = score;
    spawnPopup(boss.x + BOSS_W / 2.0, boss.y + 2, 50 * damage);
    spawnParticles(boss.x + BOSS_W / 2.0, boss.y + BOSS_H, QColor(180, 90, 255), 10);
    sound.play(SoundManager::SFX_BOSS_HIT);
    shakeTimer = std::fmax(shakeTimer, 0.1);

    if (boss.hp <= 0) {
        boss.hp = 0;
        boss.dying = true;
        boss.dyingTimer = 2.0;
        for (int i = 0; i < MAX_BALLS; ++i)    balls[i].active = false;
        for (int i = 0; i < MAX_BOLTS; ++i)    bolts[i].active = false;
        for (int i = 0; i < MAX_POWERUPS; ++i) powerups[i].active = false;
        showMessage("MORVAX FALLS!", 2.5, QColor(255, 220, 90));
        return;
    }

    const int phase = boss.hp <= boss.maxHp / 3 ? 2 : (boss.hp <= boss.maxHp * 2 / 3 ? 1 : 0);
    if (phase > boss.phase) {
        boss.phase = phase;
        flashTimer = 0.15;
        showMessage(phase == 1 ? "MORVAX IS ANGRY!" : "MORVAX IS FURIOUS!", 2.0, QColor(220, 120, 255));
        // he rebuilds his shield wall, tougher each time
        for (int c = 2; c < COLS - 2; ++c) {
            Brick &b = bricks[SHIELD_ROW][c];
            if (b.alive) continue;
            b.alive = true;
            b.solid = b.heart = b.bomb = b.ghost = false;
            b.hp = b.maxHp = phase + 1;
            ++bricksLeft;
            spawnParticles(brickLeft(c) + BRICK_W / 2.0, brickTop(SHIELD_ROW) + 4, QColor(150, 70, 220), 4);
        }
    }
}

void GameWidget::spawnBolt(double x, double y, double vx, double vy)
{
    for (int i = 0; i < MAX_BOLTS; ++i) {
        if (bolts[i].active) continue;
        bolts[i].active = true;
        bolts[i].x = x;
        bolts[i].y = y;
        bolts[i].vx = vx;
        bolts[i].vy = vy;
        return;
    }
}

void GameWidget::updateBolts(double dt)
{
    for (int i = 0; i < MAX_BOLTS; ++i) {
        Bolt &o = bolts[i];
        if (!o.active) continue;
        o.x += o.vx * dt;
        o.y += o.vy * dt;
        if (o.y > H + 4 || o.x < LEFT || o.x > RIGHT) {
            o.active = false;
            continue;
        }
        if (o.y + 2 >= PADDLE_Y && o.y - 2 <= PADDLE_Y + PADDLE_H
                && std::fabs(o.x - paddleX) <= paddleW / 2.0 + 2.0) {
            o.active = false;
            freezeTimer = 1.2;
            combo = 0;
            shakeTimer = std::fmax(shakeTimer, 0.2);
            spawnParticles(o.x, PADDLE_Y, QColor(170, 230, 255), 14);
            sound.play(SoundManager::SFX_FREEZE);
            showMessage("FROZEN!", 1.0, QColor(170, 230, 255));
        }
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
            if (isCurse(pu.type)) {
                sound.play(SoundManager::SFX_CURSE);
                shakeTimer = std::fmax(shakeTimer, 0.15);
                spawnParticles(pu.x, PADDLE_Y, QColor(255, 60, 60), 12);
            } else {
                score += 50;
                if (score > highScore) highScore = score;
                spawnPopup(pu.x, PADDLE_Y - 6, 50);
                sound.play(SoundManager::SFX_POWERUP);
            }
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
        // lasers burn through shadow bolts and hurt Morvax
        for (int k = 0; k < MAX_BOLTS; ++k) {
            if (bolts[k].active && std::fabs(bolts[k].x - l.x) <= 2.5 && std::fabs(bolts[k].y - l.y) <= 4.0) {
                bolts[k].active = false;
                l.active = false;
                spawnParticles(l.x, l.y, QColor(180, 90, 255), 6);
            }
        }
        if (!l.active) continue;
        if (boss.active && !boss.dying && l.x >= boss.x && l.x < boss.x + BOSS_W
                && l.y >= boss.y && l.y < boss.y + BOSS_H) {
            l.active = false;
            bossHit(1);
            continue;
        }

        bool hit = false;
        for (int r = 0; r < ROWS && !hit; ++r) {
            for (int c = 0; c < COLS && !hit; ++c) {
                if (!bricks[r][c].alive || !ghostSolid(r, c)) continue;
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
    for (int i = 0; i < MAX_POPUPS; ++i) {
        Popup &u = popups[i];
        if (!u.active) continue;
        u.life -= dt;
        u.y -= 16.0 * dt;
        if (u.life <= 0.0) u.active = false;
    }
    for (int i = 0; i < MAX_ZAPS; ++i) {
        if (!zaps[i].active) continue;
        zaps[i].life -= dt;
        if (zaps[i].life <= 0.0) zaps[i].active = false;
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
    if (b.bomb)  return QColor(0xb8, 0x3a, 0x2a);
    if (b.ghost) return QColor(0xc4, 0xb8, 0xff);
    if (b.hp >= 4) return QColor(0x96, 0x46, 0xdc);
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

double GameWidget::toLogicalY(double widgetY) const
{
    return (widgetY - viewOffsetY()) / double(viewScale());
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void GameWidget::handleMenuKey(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Up:
    case Qt::Key_W:
        menuSel = (menuSel + MENU_ITEMS - 1) % MENU_ITEMS;
        sound.play(SoundManager::SFX_WALL);
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        menuSel = (menuSel + 1) % MENU_ITEMS;
        sound.play(SoundManager::SFX_WALL);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (!e->isAutoRepeat()) activateMenu(menuSel);
        break;
    case Qt::Key_M:
        if (!e->isAutoRepeat()) activateMenu(MI_SOUND);
        break;
    default:
        break;
    }
}

// How-to-play and high-score screens
void GameWidget::handleInfoKey(QKeyEvent *e)
{
    const int k = e->key();
    if (e->isAutoRepeat()) return;

    if (k == Qt::Key_M) {
        activateMenu(MI_SOUND);
        return;
    }
    if (state == HowTo && (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_A || k == Qt::Key_D)) {
        howToPage = 1 - howToPage;
        sound.play(SoundManager::SFX_WALL);
        return;
    }
    if (state == Scores && (k == Qt::Key_Delete || k == Qt::Key_Backspace)) {
        if (clearArmed) {                              // second press: really clear
            clearScores();
            clearArmed = false;
            scoreHighlight = -1;
            sound.play(SoundManager::SFX_LIFE_LOST);
        } else {
            clearArmed = true;
            sound.play(SoundManager::SFX_PAUSE);
        }
        return;
    }
    if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space || k == Qt::Key_Escape) {
        goMenu();
        sound.play(SoundManager::SFX_RESUME);
        return;
    }
    clearArmed = false;                                // any other key cancels the clear
}

// typing your initials after a high score
void GameWidget::handleNameKey(QKeyEvent *e)
{
    const int k = e->key();

    if (k >= Qt::Key_A && k <= Qt::Key_Z) {
        nameIdx[nameCursor] = k - Qt::Key_A;
        if (nameCursor < 2) ++nameCursor;
        sound.play(SoundManager::SFX_PADDLE);
        return;
    }
    if (k >= Qt::Key_0 && k <= Qt::Key_9) {
        nameIdx[nameCursor] = 26 + (k - Qt::Key_0);
        if (nameCursor < 2) ++nameCursor;
        sound.play(SoundManager::SFX_PADDLE);
        return;
    }

    switch (k) {
    case Qt::Key_Up:
        nameIdx[nameCursor] = (nameIdx[nameCursor] + 1) % NAME_COUNT;
        sound.play(SoundManager::SFX_WALL);
        break;
    case Qt::Key_Down:
        nameIdx[nameCursor] = (nameIdx[nameCursor] + NAME_COUNT - 1) % NAME_COUNT;
        sound.play(SoundManager::SFX_WALL);
        break;
    case Qt::Key_Left:
    case Qt::Key_Backspace:
        if (nameCursor > 0) --nameCursor;
        break;
    case Qt::Key_Right:
        if (nameCursor < 2) ++nameCursor;
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!e->isAutoRepeat()) confirmName();
        break;
    case Qt::Key_Escape:
        if (!e->isAutoRepeat()) goMenu(0);             // skip: the score is not saved
        break;
    default:
        break;
    }
}

// world map: choose a level along the path
void GameWidget::handleMapKey(QKeyEvent *e)
{
    const int k = e->key();
    int sel = mapSel;
    switch (k) {
    case Qt::Key_Up:
    case Qt::Key_Right:
    case Qt::Key_W:
    case Qt::Key_D:
        if (sel < Story::LEVELS) ++sel;
        break;
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_S:
    case Qt::Key_A:
        if (sel > 1) --sel;
        break;
    case Qt::Key_PageUp:
        sel = sel + Story::LEVELS_PER_CHAPTER > Story::LEVELS ? Story::LEVELS : sel + Story::LEVELS_PER_CHAPTER;
        break;
    case Qt::Key_PageDown:
        sel = sel - Story::LEVELS_PER_CHAPTER < 1 ? 1 : sel - Story::LEVELS_PER_CHAPTER;
        break;
    case Qt::Key_Home:
        sel = 1;
        break;
    case Qt::Key_End:
        sel = Story::LEVELS;
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (!e->isAutoRepeat()) playSelected();
        return;
    case Qt::Key_Escape:
        if (!e->isAutoRepeat()) {
            goMenu(MI_STORY);
            sound.play(SoundManager::SFX_RESUME);
        }
        return;
    case Qt::Key_M:
        if (!e->isAutoRepeat()) activateMenu(MI_SOUND);
        return;
    default:
        return;
    }
    if (sel != mapSel) {
        mapSel = sel;
        messageTimer = 0.0;
        sound.play(SoundManager::SFX_WALL);
    }
}

// story scene: SPACE shows the whole page / turns the page, ESC skips the scene
void GameWidget::handleSceneKey(QKeyEvent *e)
{
    if (e->isAutoRepeat()) return;
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        advanceScene();
        break;
    case Qt::Key_Escape:
        skipScene();
        break;
    case Qt::Key_M:
        activateMenu(MI_SOUND);
        break;
    default:
        break;
    }
}

void GameWidget::keyPressEvent(QKeyEvent *e)
{
    if (state == Menu)      { handleMenuKey(e); return; }
    if (state == EnterName) { handleNameKey(e); return; }
    if (state == HowTo || state == Scores) { handleInfoKey(e); return; }
    if (state == WorldMap)  { handleMapKey(e); return; }
    if (state == Scene)     { handleSceneKey(e); return; }
    if (state == Ending) {
        const int k = e->key();
        if (!e->isAutoRepeat() && (k == Qt::Key_Space || k == Qt::Key_Return
                                   || k == Qt::Key_Enter || k == Qt::Key_Escape))
            primaryAction();
        return;
    }

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
        if (!e->isAutoRepeat()) togglePause();
        break;
    case Qt::Key_Escape:
        if (e->isAutoRepeat()) break;
        if (state == Playing || state == Paused) togglePause();
        else if (state == Ready) finishRun();
        else if ((state == GameOver || state == Won) && endLock <= 0.0) finishRun();
        else if (state == LevelClear && storyMode) primaryAction();
        break;
    case Qt::Key_Q:
        if (state == Paused && !e->isAutoRepeat()) finishRun();   // quit to the menu
        break;
    case Qt::Key_R:
        if (e->isAutoRepeat()) break;
        if (storyMode)                         startStoryLevel(level, false);   // replay this level
        else if (state == GameOver || state == Won) primaryAction();   // keeps the high-score flow
        else                                   newGame();
        break;
    case Qt::Key_M:
        if (!e->isAutoRepeat()) {
            sound.toggleMuted();
            if (!sound.isMuted()) sound.play(SoundManager::SFX_START);
        }
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
    const double lx = toLogicalX(mouseXOf(e));

    if (state == Menu) {                               // hover highlights a menu item
        const int it = menuItemAt(lx, toLogicalY(mouseYOf(e)));
        if (it >= 0 && it != menuSel) {
            menuSel = it;
            sound.play(SoundManager::SFX_WALL);
        }
        return;
    }
    if ((state == Ready || state == Playing) && freezeTimer <= 0.0) {
        const double x = reverseTimer > 0.0 ? W - lx : lx;   // curse: mirrored
        paddleX = clampd(x, LEFT + paddleW / 2.0, RIGHT - paddleW / 2.0);
    }
}

void GameWidget::mousePressEvent(QMouseEvent *e)
{
    if (state == Menu) {
        const int it = menuItemAt(toLogicalX(mouseXOf(e)), toLogicalY(mouseYOf(e)));
        if (it >= 0) {
            menuSel = it;
            activateMenu(it);
        }
        return;
    }
    if (state == EnterName) return;                    // typing initials: keyboard only
    if (state == WorldMap) {                           // click a level: select, click again: play
        const int n = mapNodeAt(toLogicalX(mouseXOf(e)), toLogicalY(mouseYOf(e)));
        if (n < 1) return;
        if (n == mapSel) {
            playSelected();
        } else {
            mapSel = n;
            messageTimer = 0.0;
            sound.play(SoundManager::SFX_WALL);
        }
        return;
    }
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

    const int best = storyMode ? (score > storyBest[level] ? score : storyBest[level]) : highScore;
    drawText(p, QString("SCORE ") + QString::number(score), 4, 2, 1, QColor(255, 255, 255));
    drawText(p, QString("BEST ") + QString::number(best), 84, 2, 1, QColor(150, 160, 190));

    // lives as pixel hearts (right side)
    for (int i = 0; i < lives; ++i)
        drawHeart(p, W - 11 - i * 9, 2);

    drawText(p, QString("LV ") + QString::number(level) + "/"
                + QString::number(storyMode ? int(Story::LEVELS) : NUM_LEVELS),
             4, 9, 1, storyMode ? chapterColor(Story::chapterOf(level)) : QColor(200, 210, 230));

    int mult = 1 + combo / 4;
    if (mult > 5) mult = 5;
    if (mult > 1)
        drawText(p, QString("X") + QString::number(mult), 48, 9, 1, QColor(255, 210, 80));

    // active power-ups and curses: letter + seconds left (barrier: charges left)
    struct Fx { double t; int type; };
    const Fx fx[10] = {
        { wideTimer,    PU_WIDE },
        { slowTimer,    PU_SLOW },
        { laserTimer,   PU_LASER },
        { stickyTimer,  PU_STICKY },
        { fireTimer,    PU_FIRE },
        { double(barrier), PU_BARRIER },
        { tinyTimer,    PD_TINY },
        { fastTimer,    PD_FAST },
        { reverseTimer, PD_REVERSE },
        { darkTimer,    PD_DARK }
    };
    int ex = 70;
    const int exMax = sound.isMuted() ? 172 : 194;
    for (int i = 0; i < 10 && ex + 11 <= exMax; ++i) {
        if (fx[i].t <= 0.0) continue;
        drawText(p, QString(QChar(powerLetter(fx[i].type))) + QString::number(int(std::ceil(fx[i].t))),
                 ex, 9, 1, powerColor(fx[i].type));
        ex += 17;
    }

    if (sound.isMuted())
        drawText(p, "MUTE", W - 19, 9, 1, QColor(255, 110, 110));
}

void GameWidget::renderFrame()
{
    QPainter p(&frame);                  // antialiasing stays off: hard pixel edges

    drawPlayfieldFrame(p);                // background, grid dots, solid walls

    // menu, high-score and name-entry screens have their own look
    if (isMenuState()) {
        renderMenuScreen(p);
        return;
    }
    if (state == WorldMap) { drawMapScreen(p);    return; }
    if (state == Scene)    { drawSceneScreen(p);  return; }
    if (state == Ending)   { drawEndingScreen(p); return; }

    drawBricks(p);
    drawBoss(p);

    // barrier: one glowing line per charge, under the paddle
    for (int i = 0; i < barrier; ++i) {
        const QColor bc = (int(animTime * 6.0) + i) % 2 ? powerColor(PU_BARRIER) : QColor(255, 255, 255);
        p.fillRect(LEFT, int(BARRIER_Y) + i * 2, RIGHT - LEFT, 1, bc);
    }

    // paddle
    drawPaddle(p, paddleX, paddleW, int(PADDLE_Y), laserTimer > 0.0, stickyTimer > 0.0,
               freezeTimer > 0.0);

    // lasers
    for (int i = 0; i < MAX_LASERS; ++i) {
        if (!lasers[i].active) continue;
        p.fillRect(int(lasers[i].x), int(lasers[i].y), 1, 5, QColor(255, 150, 90));
        p.fillRect(int(lasers[i].x), int(lasers[i].y), 1, 1, QColor(255, 255, 255));
    }

    // Morvax's shadow bolts
    for (int i = 0; i < MAX_BOLTS; ++i) {
        if (!bolts[i].active) continue;
        const int x = int(bolts[i].x), y = int(bolts[i].y);
        p.fillRect(x - int(bolts[i].vx * 0.04), y - int(bolts[i].vy * 0.04), 1, 1, QColor(110, 50, 160));
        p.fillRect(x - 1, y - 1, 3, 3, QColor(170, 90, 240));
        p.fillRect(x, y, 1, 1, QColor(20, 5, 30));
    }

    // balls (4 x 4 pixel squares); a fireball glows orange
    for (int i = 0; i < MAX_BALLS; ++i) {
        const Ball &b = balls[i];
        if (!b.active) continue;
        const int x = int(std::floor(b.x + 0.5)) - 2;
        const int y = int(std::floor(b.y + 0.5)) - 2;
        if (fireTimer > 0.0) {
            p.fillRect(x, y, 4, 4, QColor(255, 140, 40));
            p.fillRect(x + 1, y + 1, 2, 2, QColor(255, 250, 180));
        } else {
            p.fillRect(x, y, 4, 4, QColor(255, 255, 255));
            p.fillRect(x + 3, y + 3, 1, 1, QColor(160, 190, 255));
        }
    }

    // ZAP lightning: a jagged line from the top wall down to the struck brick
    for (int i = 0; i < MAX_ZAPS; ++i) {
        const Zap &z = zaps[i];
        if (!z.active) continue;
        int x = int(z.x);
        for (int y = TOP; y < int(z.y); y += 3) {
            x += ((y * 7 + i * 13 + int(animTime * 30.0)) % 5) - 2;
            x = int(clampd(x, z.x - 6, z.x + 6));
            p.fillRect(x - 1, y, 3, 3, powerColor(PU_ZAP));
            p.fillRect(x, y, 1, 3, QColor(255, 255, 255));
        }
    }

    drawParticles(p);

    if (darkTimer > 0.0) drawDarkness(p);

    // power-up capsules (13 x 7 pixels, letter inside) glow, even in the dark
    const bool capBlink = int(animTime * 8.0) % 2 == 0;
    for (int i = 0; i < MAX_POWERUPS; ++i) {
        if (!powerups[i].active) continue;
        drawCapsule(p, int(powerups[i].x) - 6, int(powerups[i].y) - 3, powerups[i].type, capBlink);
    }

    // floating score numbers
    for (int i = 0; i < MAX_POPUPS; ++i) {
        const Popup &u = popups[i];
        if (!u.active) continue;
        const QString t = QString("+") + QString::number(u.value);
        const int x = int(u.x) - textWidth(t, 1) / 2;
        drawText(p, t, x + 1, int(u.y) + 1, 1, QColor(0, 0, 0));
        drawText(p, t, x, int(u.y), 1, u.value >= 100 ? QColor(255, 140, 200) : QColor(255, 230, 120));
    }

    if (flashTimer > 0.0)
        p.fillRect(0, HUD_H, W, H - HUD_H, QColor(255, 255, 255, int(clampd(flashTimer * 900.0, 0.0, 160.0))));

    drawHud(p);

    // floating message
    if (messageTimer > 0.0 && state == Playing)
        drawTextCentered(p, message, 170, 2, messageColor);

    // state screens
    const bool newHigh = qualifies(score);
    const bool blinkOn = int(animTime * 4.0) % 2 == 0;
    switch (state) {
    case Menu:
    case HowTo:
    case Scores:
    case EnterName:
    case WorldMap:
    case Scene:
    case Ending:
        break;                      // drawn by their own screens above
    case Ready:
        drawTextCentered(p, QString("LEVEL ") + QString::number(level), 150, 3, QColor(255, 255, 255));
        if (storyMode)
            drawTextCentered(p, Story::level(level).name, 170, 1, chapterColor(Story::chapterOf(level)));
        drawTextCentered(p, "PRESS SPACE OR CLICK TO LAUNCH", 182, 1, QColor(200, 210, 230));
        drawTextCentered(p, storyMode ? "ESC: MAP" : "ESC: MENU", 194, 1, QColor(120, 130, 170));
        break;
    case Paused:
        drawOverlay(p, "PAUSED", "", "", storyMode ? "P: RESUME  R: RESTART  Q: MAP" : "P: RESUME   Q: MENU");
        break;
    case LevelClear:
        if (storyMode) {
            drawStoryClear(p);
            break;
        }
        drawOverlay(p, "LEVEL CLEARED!",
                    QString("BONUS +") + QString::number(lastBonus),
                    QString("SCORE ") + QString::number(score),
                    "PRESS SPACE FOR THE NEXT LEVEL");
        break;
    case GameOver:
        if (storyMode) {
            drawOverlay(p, "LEVEL FAILED",
                        QString("SCORE ") + QString::number(score),
                        Story::level(level).name,
                        "SPACE: TRY AGAIN   ESC: MAP",
                        chapterColor(Story::chapterOf(level)));
            break;
        }
        drawOverlay(p, "GAME OVER",
                    QString("SCORE ") + QString::number(score),
                    newHigh ? QString("NEW HIGH SCORE!") : QString(QString("BEST ") + QString::number(highScore)),
                    newHigh ? "SPACE: ENTER YOUR NAME" : "SPACE: AGAIN   ESC: MENU",
                    newHigh ? (blinkOn ? QColor(255, 220, 90) : QColor(255, 255, 255))
                            : QColor(200, 210, 230));
        break;
    case Won:
        drawOverlay(p, "YOU WIN!",
                    QString("SCORE ") + QString::number(score),
                    newHigh ? QString("NEW HIGH SCORE!") : QString(QString("BEST ") + QString::number(highScore)),
                    newHigh ? "SPACE: ENTER YOUR NAME" : "SPACE: AGAIN   ESC: MENU",
                    newHigh ? (blinkOn ? QColor(255, 220, 90) : QColor(255, 255, 255))
                            : QColor(200, 210, 230));
        break;
    case Playing:
        break;
    }
}

void GameWidget::drawBricks(QPainter &p)
{
    // bricks (16 x 8 pixels each, sitting exactly on the grid)
    const bool blink = int(animTime * 6.0) % 2 == 0;
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            const Brick &b = bricks[r][c];
            if (!b.alive) continue;
            int kind = BK_NORMAL;
            if (b.solid)      kind = BK_SOLID;
            else if (b.heart) kind = BK_HEART;
            else if (b.bomb)  kind = BK_BOMB;
            else if (b.ghost) kind = BK_GHOST;
            drawBrickCell(p, brickLeft(c), brickTop(r), brickColor(r, c), kind,
                          kind == BK_NORMAL && b.hp < b.maxHp, !ghostSolid(r, c), blink);
        }
    }
}

void GameWidget::drawBoss(QPainter &p)
{
    if (!boss.active) return;

    // health bar along the top wall, with a skull
    const int bx = LEFT + 8, bw = RIGHT - LEFT - 10;
    drawSprite(p, SKULL, 5, LEFT + 1, TOP + 1, QColor(220, 120, 255));
    p.fillRect(bx, TOP + 2, bw, 3, QColor(40, 10, 50));
    p.fillRect(bx, TOP + 2, bw * boss.hp / boss.maxHp, 3,
               boss.phase >= 2 ? QColor(255, 60, 90) : QColor(190, 80, 255));
    p.fillRect(bx, TOP + 2, bw * boss.hp / boss.maxHp, 1, QColor(255, 200, 255));

    // a dark aura that flickers; brighter when he is angry
    const int bob = int(std::floor(std::sin(animTime * 3.0) * 1.5 + 0.5));
    const int x = int(boss.x + 0.5), y = int(boss.y + 0.5) + bob;
    if (int(animTime * 10.0) % 2 == 0 || boss.phase > 0)
        p.fillRect(x - 2, y + 4, BOSS_W + 4, BOSS_H - 4,
                   QBrush(boss.phase >= 2 ? QColor(160, 30, 60) : QColor(70, 20, 110), Qt::Dense5Pattern));

    if (boss.dying && int(animTime * 20.0) % 2 == 0) return;   // blinks while exploding
    drawPixelArt(p, BOSS_ART, 16, x, y, 1, BOSS_PAL, 7, boss.hitFlash > 0.0);
}

// DARKNESS curse: only the area around the balls and the paddle stays visible
void GameWidget::drawDarkness(QPainter &p)
{
    const double inner = 24.0 * 24.0, outer = 31.0 * 31.0;
    for (int y = TOP; y < H; y += 4) {
        for (int x = LEFT; x < RIGHT; x += 4) {
            const double cx = x + 2.0, cy = y + 2.0;
            double dx = std::fmax(0.0, std::fabs(cx - paddleX) - paddleW / 2.0);
            double dy = cy - PADDLE_Y;
            double best = dx * dx + dy * dy;
            for (int i = 0; i < MAX_BALLS; ++i) {
                if (!balls[i].active) continue;
                dx = cx - balls[i].x;
                dy = cy - balls[i].y;
                const double d2 = dx * dx + dy * dy;
                if (d2 < best) best = d2;
            }
            if (best > outer)      p.fillRect(x, y, 4, 4, QColor(4, 4, 10));
            else if (best > inner) p.fillRect(x, y, 4, 4, QBrush(QColor(4, 4, 10), Qt::Dense3Pattern));
        }
    }
}

// story mode level-clear screen: stars pop in one at a time
void GameWidget::drawStoryClear(QPainter &p)
{
    const bool on = int(animTime * 4.0) % 2 == 0;
    dimScreen(p);
    if (isBossLevel())
        drawTextCentered(p, "MORVAX DEFEATED!", 52, 2, QColor(255, 220, 90));
    else
        drawTextCentered(p, "LEVEL CLEAR!", 52, 3, QColor(255, 220, 90));

    for (int k = 0; k < 3; ++k) {
        const int x = W / 2 - 24 + k * 24 - 7;
        const int y = 78;
        drawSprite(p, STAR7, 7, x + 1, y + 1, QColor(0, 0, 0), 2);
        if (k < starsShown) {
            drawSprite(p, STAR7, 7, x, y, QColor(255, 205, 50), 2);
            p.fillRect(x + 6, y + 4, 2, 2, QColor(255, 250, 200));
        } else {
            drawSprite(p, STAR7, 7, x, y, QColor(50, 55, 90), 2);
        }
    }

    drawTextCentered(p, QString("SCORE ") + QString::number(score), 104, 2, QColor(255, 255, 255));
    drawTextCentered(p, QString("CLEAR BONUS +") + QString::number(lastBonus), 122, 1, QColor(200, 210, 230));
    if (newBest)
        drawTextCentered(p, "NEW BEST!", 134, 1, on ? QColor(255, 220, 90) : QColor(255, 255, 255));
    else
        drawTextCentered(p, QString("BEST ") + QString::number(storyBest[level]), 134, 1, QColor(150, 160, 190));
    if (starsEarned < 3)
        drawTextCentered(p, "CLEAR WITHOUT LOSING A LIFE FOR 3 STARS", 148, 1, QColor(120, 130, 170));

    if (endLock <= 0.0) {
        drawTextCentered(p, "SPACE: CONTINUE   R: REPLAY", 164, 1, on ? QColor(255, 220, 90) : QColor(200, 210, 230));
    }
}

void GameWidget::drawParticles(QPainter &p)
{
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle &q = particles[i];
        if (!q.active) continue;
        const int size = (q.life > q.maxLife * 0.4) ? 2 : 1;
        p.fillRect(int(q.x), int(q.y), size, size, q.color);
    }
}

// ---------------------------------------------------------------------------
// Menu, high-score, how-to-play and name-entry screens
// ---------------------------------------------------------------------------

void GameWidget::renderMenuScreen(QPainter &p)
{
    // top strip: the current record, and a mute marker
    p.fillRect(0, 0, W, HUD_H, QColor(8, 9, 20));
    QString hi("HI-SCORE 0");
    if (scoreCount > 0)
        hi = QString("HI-SCORE ") + QString::number(scores[0].score) + " " + QString(scores[0].name);
    drawText(p, hi, (W - textWidth(hi, 1)) / 2, 5, 1, QColor(255, 215, 90));
    if (sound.isMuted())
        drawText(p, "MUTE", W - 19, 5, 1, QColor(255, 110, 110));

    drawStars(p, animTime);

    switch (state) {
    case Menu:      drawMenu(p);         break;
    case Scores:    drawScoresScreen(p); break;
    case HowTo:     drawHowToScreen(p);  break;
    case EnterName: drawNameScreen(p);   break;
    default:        break;
    }

    drawParticles(p);
}

void GameWidget::drawMenu(QPainter &p)
{
    const bool on = int(animTime * 4.0) % 2 == 0;

    // demo ball with a fading trail, and its helper paddle
    for (int i = TRAIL - 1; i >= 0; --i) {
        const int v = 36 + (TRAIL - i) * 9;
        p.fillRect(int(trailX[i]) - 1, int(trailY[i]) - 1, 2, 2, QColor(v, v + 10, v + 55));
    }
    p.fillRect(int(std::floor(mbx + 0.5)) - 2, int(std::floor(mby + 0.5)) - 2, 4, 4, QColor(255, 255, 255));
    p.fillRect(int(std::floor(mbx + 0.5)) + 1, int(std::floor(mby + 0.5)) + 1, 1, 1, QColor(160, 190, 255));
    drawPaddle(p, demoPadX, PADDLE_W_BASE, int(DEMO_PADDLE_Y), false, false);

    // decorative bricks with a moving shine; they flash when the ball taps them
    for (int r = 0; r < DECOR_ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            QColor col = rowColor(r * 2);
            if (flash[c] > 0.0 && r == DECOR_ROWS - 1)  col = col.lighter(175);
            else if ((c + r * 2 + int(animTime * 10.0)) % 24 == 0) col = col.lighter(150);
            drawBrick(p, LEFT + c * BRICK_W, DECOR_Y + r * BRICK_H, col);
        }
    }

    drawLogo(p, animTime, 30);
    drawTextCentered(p, "* PIXEL EDITION *", 52, 1, QColor::fromHsv(int(animTime * 90) % 360, 120, 255));

    // menu items
    for (int i = 0; i < MENU_ITEMS; ++i) {
        const int y = MENU_Y0 + i * MENU_STEP;
        const bool sel = (i == menuSel);
        if (sel) {
            p.fillRect(40, y - 2, 128, 14, QColor(28, 38, 96));
            p.fillRect(40, y - 2, 128, 1, QColor(90, 110, 220));
            p.fillRect(40, y + 11, 128, 1, QColor(90, 110, 220));
            const int off = on ? 0 : 2;
            drawText(p, ">", 46 + off, y, 2, QColor(255, 220, 90));
            drawText(p, "<", 154 - off, y, 2, QColor(255, 220, 90));
        }
        drawTextCentered(p, menuLabel(i), y, 2, sel ? QColor(255, 220, 90) : QColor(185, 195, 230));
    }

    drawTextCentered(p, "UP/DOWN + ENTER OR CLICK", 198, 1, QColor(120, 130, 170));
    drawTextCentered(p, "M: SOUND ON/OFF", 210, 1, QColor(120, 130, 170));
}

void GameWidget::drawScoresScreen(QPainter &p)
{
    const bool on = int(animTime * 4.0) % 2 == 0;

    drawTrophy(p, 36, 27);
    drawTrophy(p, W - 36 - 9, 27);
    drawTextCentered(p, "HIGH SCORES", 28, 2, QColor(255, 220, 90));
    p.fillRect(LEFT + 8, 43, RIGHT - LEFT - 16, 1, QColor(90, 100, 160));

    const QColor head(120, 130, 170);
    drawText(p, "RK",    16, 49, 1, head);
    drawText(p, "NAME",  40, 49, 1, head);
    drawText(p, "SCORE", 97, 49, 1, head);
    drawText(p, "LV",   126, 49, 1, head);
    drawText(p, "DATE", 146, 49, 1, head);

    for (int i = 0; i < MAX_SCORES; ++i) {
        const int y = 62 + i * 13;
        QColor col(160, 175, 215);
        if (i == 0)      col = QColor(255, 215, 0);          // gold
        else if (i == 1) col = QColor(205, 210, 225);        // silver
        else if (i == 2) col = QColor(214, 140, 70);         // bronze

        if (i == scoreHighlight) {                           // the score you just set flashes
            if (on) p.fillRect(LEFT + 4, y - 3, RIGHT - LEFT - 8, 11, QColor(40, 50, 120));
            col = on ? QColor(255, 255, 255) : QColor(255, 220, 90);
        }

        drawText(p, QString::number(i + 1) + ".", 16, y, 1, col);
        if (i < scoreCount) {
            const ScoreEntry &e = scores[i];
            const QString sc = QString::number(e.score);
            drawText(p, QString(e.name), 40, y, 1, col);
            drawText(p, sc, 116 - textWidth(sc, 1), y, 1, col);
            drawText(p, QString("L") + QString::number(e.level), 126, y, 1, col);
            drawText(p, QString(e.date), 146, y, 1, col);
            if (i == 0)
                drawText(p, "*", 185, y, 1, on ? QColor(255, 255, 255) : QColor(255, 215, 0));
        } else {
            drawText(p, "---", 40, y, 1, QColor(60, 68, 105));
        }
    }

    if (clearArmed)
        drawTextCentered(p, "PRESS DEL AGAIN TO CLEAR", 196, 1, on ? QColor(255, 110, 110) : QColor(255, 255, 255));
    else
        drawTextCentered(p, "ENTER: BACK   DEL: CLEAR", 196, 1, QColor(200, 210, 230));
    drawTextCentered(p, "SAVED ON THIS COMPUTER", 210, 1, QColor(110, 120, 160));
}

void GameWidget::drawHowToScreen(QPainter &p)
{
    const QColor txt(200, 210, 230), gold(255, 220, 90), dim(150, 160, 190);

    drawTextCentered(p, "HOW TO PLAY", 26, 2, gold);
    drawText(p, QString::number(howToPage + 1) + "/2", RIGHT - 16, 30, 1, QColor(120, 130, 170));

    if (howToPage == 0) {
        drawTextCentered(p, "MOVE: MOUSE OR ARROWS OR A/D", 44, 1, txt);
        drawTextCentered(p, "SPACE: LAUNCH BALL / FIRE LASERS", 55, 1, txt);
        drawTextCentered(p, "P: PAUSE  R: RESTART  M: MUTE", 66, 1, txt);
        drawTextCentered(p, "BREAK ALL BRICKS TO CLEAR A LEVEL", 82, 1, QColor(255, 255, 255));

        struct Row { QColor col; int kind; bool faded; const char *label; };
        const Row rows[8] = {
            { rowColor(0),              BK_NORMAL, false, "ONE HIT" },
            { QColor(0xe8, 0x9a, 0x2c), BK_NORMAL, false, "TWO HITS" },
            { QColor(0xd9, 0x43, 0x3f), BK_NORMAL, false, "THREE HITS" },
            { QColor(0x96, 0x46, 0xdc), BK_NORMAL, false, "FOUR HITS" },
            { QColor(0x7a, 0x80, 0x92), BK_SOLID,  false, "SOLID: CANNOT BREAK" },
            { QColor(0xd0, 0x40, 0x70), BK_HEART,  false, "HEART: EXTRA LIFE" },
            { QColor(0xb8, 0x3a, 0x2a), BK_BOMB,   false, "BOMB: BLOWS UP AROUND IT" },
            { QColor(0xc4, 0xb8, 0xff), BK_GHOST,  int(animTime * 1.5) % 2 == 1, "GHOST: FADES IN AND OUT" }
        };
        for (int i = 0; i < 8; ++i) {
            const int y = 96 + i * 13;
            drawBrickCell(p, 24, y - 1, rows[i].col, rows[i].kind, false, rows[i].faded,
                          int(animTime * 6.0) % 2 == 0);
            drawText(p, rows[i].label, 46, y + 1, 1, txt);
        }
        drawTextCentered(p, "STORY: 3 STARS = NO LIVES LOST", 206, 1, dim);
    } else {
        drawText(p, "CATCH THESE", 16, 44, 1, QColor(120, 230, 120));
        drawText(p, "DODGE CURSES", 112, 44, 1, QColor(255, 90, 90));
        for (int t = 0; t < PU_COUNT; ++t) {
            const bool curse = isCurse(t);
            const int col = curse ? 1 : 0;
            const int row = curse ? t - PD_TINY : t;
            const int x = col ? 113 : 17;
            const int y = 56 + row * 13;
            drawCapsule(p, x, y - 1, t, curse && int(animTime * 8.0) % 2 == 0);
            drawText(p, powerName(t), x + 18, y, 1, curse ? QColor(255, 150, 150) : txt);
        }
        drawTextCentered(p, "FIRE: BURNS THROUGH BRICKS", 166, 1, dim);
        drawTextCentered(p, "BARRIER: SAVES A FALLING BALL", 177, 1, dim);
        drawTextCentered(p, "ZAP: LIGHTNING HITS 4 BRICKS", 188, 1, dim);
        drawTextCentered(p, "CURSES HAVE A SKULL: AVOID THEM!", 204, 1, QColor(255, 120, 120));
    }
    drawTextCentered(p, "LEFT/RIGHT: PAGE   ENTER: BACK", 224, 1, QColor(120, 130, 170));
}

void GameWidget::drawNameScreen(QPainter &p)
{
    const bool on = int(animTime * 4.0) % 2 == 0;
    const int rank = scoreRank(score);                      // 0 = best

    drawTextCentered(p, "NEW HIGH SCORE!", 30, 2, on ? QColor(255, 220, 90) : QColor(255, 255, 255));
    drawTextCentered(p, QString::number(score), 54, 4, QColor(255, 110, 150));
    if (rank == 0)
        drawTextCentered(p, "* NEW RECORD *", 84, 1, on ? QColor(255, 215, 0) : QColor(255, 255, 255));
    else
        drawTextCentered(p, QString("RANK ") + QString::number(rank + 1) + " OF " + QString::number(MAX_SCORES),
                         84, 1, QColor(200, 210, 230));

    drawTextCentered(p, "ENTER YOUR INITIALS", 106, 1, QColor(255, 255, 255));

    const int x0 = (W - (3 * 16 - 4)) / 2;
    const int y  = 132;
    for (int i = 0; i < 3; ++i) {
        const int x = x0 + i * 16;
        const bool cur = (i == nameCursor);
        p.fillRect(x - 2, y - 3, 16, 26, cur ? QColor(30, 40, 100) : QColor(20, 24, 52));
        const QString ch = QString(QChar(NAME_CHARS[nameIdx[i]]));
        drawText(p, ch, x + 1, y + 1, 4, QColor(0, 0, 0));
        drawText(p, ch, x, y, 4, cur ? QColor(255, 220, 90) : QColor(255, 255, 255));
        if (cur) {
            if (on) p.fillRect(x, y + 22, 12, 2, QColor(255, 220, 90));
            drawSprite(p, UP_ARROW,   3, x + 3, y - 9,  QColor(255, 220, 90));
            drawSprite(p, DOWN_ARROW, 3, x + 3, y + 26, QColor(255, 220, 90));
        }
    }

    drawTextCentered(p, "UP/DOWN: CHANGE LETTER", 172, 1, QColor(200, 210, 230));
    drawTextCentered(p, "LEFT/RIGHT: MOVE", 183, 1, QColor(200, 210, 230));
    drawTextCentered(p, "OR JUST TYPE YOUR LETTERS", 194, 1, QColor(200, 210, 230));
    drawTextCentered(p, "ENTER: SAVE   ESC: SKIP", 212, 1, on ? QColor(255, 220, 90) : QColor(150, 160, 190));
}

// ---------------------------------------------------------------------------
// Story mode: progress, the world map, dialogue scenes and the ending
// ---------------------------------------------------------------------------

void GameWidget::loadStoryProgress()
{
    QSettings st;
    storyUnlocked = st.value("story/unlocked", 1).toInt();
    if (storyUnlocked < 1) storyUnlocked = 1;
    if (storyUnlocked > Story::LEVELS) storyUnlocked = Story::LEVELS;
    storyStars[0] = storyBest[0] = 0;
    for (int n = 1; n <= Story::LEVELS; ++n) {
        storyStars[n] = st.value(QString("story/stars%1").arg(n), 0).toInt();
        storyBest[n]  = st.value(QString("story/best%1").arg(n), 0).toInt();
    }
}

void GameWidget::saveStoryResult(int lvl, int stars, int s)
{
    QSettings st;
    if (stars > storyStars[lvl]) {
        storyStars[lvl] = stars;
        st.setValue(QString("story/stars%1").arg(lvl), stars);
    }
    if (s > storyBest[lvl]) {
        storyBest[lvl] = s;
        st.setValue(QString("story/best%1").arg(lvl), s);
    }
    if (lvl + 1 > storyUnlocked && lvl < Story::LEVELS) {
        storyUnlocked = lvl + 1;
        st.setValue("story/unlocked", storyUnlocked);
    }
}

int GameWidget::unlockedCount() const
{
    return DEV_UNLOCK_ALL ? int(Story::LEVELS) : storyUnlocked;
}

int GameWidget::totalStars() const
{
    int n = 0;
    for (int i = 1; i <= Story::LEVELS; ++i) n += storyStars[i];
    return n;
}

void GameWidget::openMap(int sel, bool snap)
{
    storyMode = true;
    mapSel = sel < 1 ? 1 : (sel > Story::LEVELS ? int(Story::LEVELS) : sel);
    if (snap) mapCam = mapCamTarget(mapSel);
    state = WorldMap;
    messageTimer = 0.0;
    leftHeld = rightHeld = fireHeld = false;
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].active = false;
    // a little burst on the selected level
    spawnParticles(mapNodeX(mapSel), mapNodeY(mapSel) - mapCam + MAP_VIEW_Y,
                   chapterColor(Story::chapterOf(mapSel)).lighter(140), 14);
}

void GameWidget::playSelected()
{
    if (mapSel > unlockedCount()) {
        sound.play(SoundManager::SFX_SOLID);
        showMessage("LOCKED - CLEAR THE LEVEL BEFORE IT", 1.5, QColor(255, 120, 120));
        return;
    }
    startStoryLevel(mapSel, true);
}

void GameWidget::startStoryLevel(int lvl, bool withIntro)
{
    storyMode = true;
    level = lvl;
    mapSel = lvl;
    score = 0;
    lives = 3;
    combo = 0;
    livesLost = 0;
    paddleX = W / 2.0;
    resetEffects();
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].active = false;
    loadLevel(lvl);
    paddleW = basePaddleW();
    resetBallOnPaddle();

    const Story::Level &def = Story::level(lvl);
    if (withIntro && Story::pageCount(def.intro) > 0) {
        beginScene(def.intro, false);
    } else {
        state = Ready;
        sound.play(SoundManager::SFX_START);
    }
}

// after the level-clear screen: the outro scene (if any), then the map
void GameWidget::continueStory()
{
    const Story::Level &def = Story::level(level);
    if (Story::pageCount(def.outro) > 0) beginScene(def.outro, true);
    else if (level >= Story::LEVELS)     beginEnding();
    else                                 openMap(level + 1, false);
}

void GameWidget::beginScene(const Story::Page *pages, bool outro)
{
    scenePages = pages;
    sceneCount = Story::pageCount(pages);
    scenePage = 0;
    sceneChars = 0.0;
    sceneIsOutro = outro;
    state = Scene;
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].active = false;
    sound.play(SoundManager::SFX_RESUME);
}

// first press shows the whole page, the next one turns the page
void GameWidget::advanceScene()
{
    const int len = scenePageLength();
    if (sceneChars < len) {
        sceneChars = len;
        return;
    }
    ++scenePage;
    sceneChars = 0.0;
    if (scenePage < sceneCount) {
        sound.play(SoundManager::SFX_WALL);
        return;
    }
    skipScene();
}

void GameWidget::skipScene()
{
    if (!sceneIsOutro) {                              // intro done: the level starts
        state = Ready;
        sound.play(SoundManager::SFX_START);
    } else if (level >= Story::LEVELS) {
        beginEnding();
    } else {
        openMap(level + 1, false);
    }
}

// the current page split into lines (narrator pages have no portrait: wider lines)
int GameWidget::wrapScenePage(QString out[], int maxLines) const
{
    if (!scenePages || scenePage >= sceneCount) return 0;
    const Story::Page &pg = scenePages[scenePage];
    return wrapText(QString(pg.text), pg.speaker == Story::NARRATOR ? 45 : 37, out, maxLines);
}

int GameWidget::scenePageLength() const
{
    QString lines[8];
    const int n = wrapScenePage(lines, 8);
    int len = 0;
    for (int i = 0; i < n; ++i) len += lines[i].length();
    return len;
}

void GameWidget::beginEnding()
{
    state = Ending;
    endLock = 1.5;
    confettiTimer = 0.0;
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].active = false;
    sound.play(SoundManager::SFX_WIN);
}

int GameWidget::mapNodeAt(double lx, double ly) const
{
    if (ly < MAP_VIEW_Y || ly >= MAP_PANEL_Y) return -1;
    for (int n = 1; n <= Story::LEVELS; ++n) {
        const double sx = mapNodeX(n);
        const double sy = mapNodeY(n) - mapCam + MAP_VIEW_Y;
        if (std::fabs(lx - sx) <= 10.0 && std::fabs(ly - sy) <= 10.0) return n;
    }
    return -1;
}

void GameWidget::drawMapScreen(QPainter &p)
{
    const int cam = int(mapCam + 0.5);
    const bool on = int(animTime * 4.0) % 2 == 0;
    const int open = unlockedCount();

    p.setClipRect(0, MAP_VIEW_Y, W, MAP_VIEW_H);

    // ---- background: one land per chapter, with a dithered border between them
    for (int sy = MAP_VIEW_Y; sy < MAP_PANEL_Y; sy += 2) {
        const int wy = sy - MAP_VIEW_Y + cam;
        const int ch = chapterAtMapY(wy);
        QColor bg = chapterColor(ch).darker(ch == 2 ? 520 : 430);
        if ((wy / 2) % 6 == 0) bg = bg.lighter(112);          // soft pixel stripes
        p.fillRect(0, sy, W, 2, bg);
    }
    for (int c = 1; c < Story::CHAPTERS; ++c) {
        const int sy = chapterBorderY(c) - cam + MAP_VIEW_Y;
        p.fillRect(0, sy, W, 6, QBrush(chapterColor(c).darker(520), Qt::Dense4Pattern));
    }
    // stars over the Shadow Spire
    for (int i = 0; i < 40; ++i) {
        const int wy = (i * 53) % chapterBorderY(2);
        const int sy = wy - cam + MAP_VIEW_Y;
        const int ph = (int(animTime * 2.5) + i * 3) % 7;
        p.fillRect((i * 137) % W, sy, 1, 1, ph == 0 ? QColor(255, 255, 255) : QColor(90, 70, 130));
    }

    // ---- decorations (trees, crystals, obsidian spikes), kept away from the path
    for (int k = 0; k < 70; ++k) {
        const unsigned h = unsigned(k) * 2654435761u;           // cheap hash: scattered, but fixed
        const int wy = 30 + int((h >> 8) % unsigned(MAP_H - 40));
        const int wx = 6 + int((h >> 20) % unsigned(W - 17));
        if (std::fabs(wx + 2 - mapPathX(wy)) < 22.0) continue;
        const int sy = wy - cam + MAP_VIEW_Y;
        if (sy < MAP_VIEW_Y - 8 || sy > MAP_PANEL_Y) continue;
        switch (chapterAtMapY(wy)) {
        case 0:  drawPixelArt(p, TREE_ART, 7, wx, sy, 1, TREE_PAL, 2);       break;
        case 1:  drawPixelArt(p, CRYSTAL_ART, 7, wx, sy, 1, CRYSTAL_PAL, 2); break;
        default: drawPixelArt(p, SPIKE_ART, 7, wx, sy, 1, SPIKE_PAL, 2);     break;
        }
    }

    // ---- the path: dotted, bright where it leads to an open level
    for (int n = 1; n < Story::LEVELS; ++n) {
        const double x0 = mapNodeX(n), y0 = mapNodeY(n);
        const double x1 = mapNodeX(n + 1), y1 = mapNodeY(n + 1);
        const int steps = int(std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) / 4.0);
        const QColor dot = (n + 1 <= open) ? QColor(250, 230, 170) : QColor(70, 70, 95);
        for (int i = 1; i < steps; ++i) {
            const double t = double(i) / steps;
            const int px = int(x0 + (x1 - x0) * t);
            const int py = int(y0 + (y1 - y0) * t) - cam + MAP_VIEW_Y;
            p.fillRect(px - 1, py, 2, 2, QColor(0, 0, 0, 90));
            p.fillRect(px - 1, py - 1, 2, 2, dot);
        }
    }

    // ---- chapter banners
    for (int c = 0; c < Story::CHAPTERS; ++c) {
        const int wy = c == 0 ? mapNodeY(1) + 22 : chapterBorderY(c);
        const int sy = wy - cam + MAP_VIEW_Y;
        const QColor acc = chapterColor(c);
        const QString t = QString("CHAPTER ") + romanNumeral(c) + " - " + Story::chapter(c).name;
        const int tw = textWidth(t, 1);
        const int bx = (W - tw) / 2 - 6;
        p.fillRect(bx - 4, sy - 3, 4, 7, acc.darker(300));             // ribbon tails
        p.fillRect(bx + tw + 12, sy - 3, 4, 7, acc.darker(300));
        p.fillRect(bx, sy - 5, tw + 12, 11, acc.darker(200));
        p.fillRect(bx, sy - 5, tw + 12, 1, acc);
        p.fillRect(bx, sy + 5, tw + 12, 1, acc.darker(350));
        drawText(p, t, bx + 6, sy - 2, 1, QColor(255, 255, 255));
    }

    // ---- the level nodes
    for (int n = 1; n <= Story::LEVELS; ++n) {
        const int sx = mapNodeX(n);
        const int sy = mapNodeY(n) - cam + MAP_VIEW_Y;
        if (sy < MAP_VIEW_Y - 20 || sy > MAP_PANEL_Y + 20) continue;

        const Story::Level &def = Story::level(n);
        const bool isOpen = n <= open;
        const bool sel = n == mapSel;
        const int r = def.boss ? 10 : 7;                       // half size of the badge
        const QColor acc = chapterColor(Story::chapterOf(n));

        QColor rim = QColor(90, 90, 110);
        if (sel)                 rim = on ? QColor(255, 255, 255) : QColor(255, 220, 90);
        else if (storyStars[n])  rim = QColor(255, 205, 60);
        else if (isOpen)         rim = acc.lighter(130);
        const QColor body = isOpen ? acc.darker(def.boss ? 300 : 230) : QColor(40, 40, 55);

        p.fillRect(sx - r + 1, sy - r + 2, 2 * r, 2 * r, QColor(0, 0, 0, 110));   // shadow
        p.fillRect(sx - r + 1, sy - r, 2 * r - 2, 2 * r, rim);
        p.fillRect(sx - r, sy - r + 1, 2 * r, 2 * r - 2, rim);
        p.fillRect(sx - r + 2, sy - r + 1, 2 * r - 4, 2 * r - 2, body);
        p.fillRect(sx - r + 1, sy - r + 2, 2 * r - 2, 2 * r - 4, body);
        p.fillRect(sx - r + 2, sy - r + 1, 2 * r - 4, 1, body.lighter(140));      // shine

        if (!isOpen) {
            drawSprite(p, LOCK, 6, sx - 2, sy - 3, QColor(150, 150, 170));
        } else if (def.boss) {
            drawSprite(p, SKULL, 5, sx - 5, sy - 5, QColor(240, 220, 255), 2);
            p.fillRect(sx - 3, sy - 1, 2, 2, QColor(255, 50, 50));             // red eyes
            p.fillRect(sx + 1, sy - 1, 2, 2, QColor(255, 50, 50));
        } else {
            const QString num = QString::number(n);
            drawText(p, num, sx - textWidth(num, 1) / 2, sy - 2, 1, QColor(255, 255, 255));
        }

        // the prism shard waiting at the end of each chapter
        if (n % Story::LEVELS_PER_CHAPTER == 0) {
            const int dx = sx < W / 2 ? r + 5 : -r - 10;
            const QColor sc = storyStars[n] ? QColor::fromHsv((int(animTime * 120) + n * 40) % 360, 120, 255)
                                            : QColor(80, 80, 100);
            drawSprite(p, SHARD, 6, sx + dx, sy - 3, sc);
        }

        // stars earned
        if (isOpen) {
            for (int k = 0; k < 3; ++k)
                drawSprite(p, STAR3, 3, sx - 5 + k * 4, sy + r + 1,
                           k < storyStars[n] ? QColor(255, 215, 60) : QColor(75, 75, 100));
        }

        if (sel) {                                              // bouncing marker
            const int bob = int(std::floor(std::sin(animTime * 6.0) * 2.0 + 0.5));
            drawSprite(p, DOWN_ARROW, 3, sx - 2, sy - r - 7 + bob, QColor(255, 220, 90));
        }
    }

    p.setClipping(false);
    drawParticles(p);

    // ---- top strip: title, stars collected, shards
    p.fillRect(0, 0, W, HUD_H, QColor(8, 9, 20));
    drawText(p, "STORY MODE", 4, 2, 1, QColor(255, 220, 90));
    const QString st = QString::number(totalStars()) + "/" + QString::number(Story::LEVELS * 3);
    drawText(p, st, W - 4 - textWidth(st, 1), 2, 1, QColor(255, 255, 255));
    drawSprite(p, STAR7, 7, W - 13 - textWidth(st, 1), 1, QColor(255, 215, 60));
    if (DEV_UNLOCK_ALL)
        drawText(p, "DEV: ALL LEVELS OPEN", 4, 9, 1, QColor(255, 110, 110));
    for (int c = 0; c < Story::CHAPTERS; ++c) {
        const bool got = storyStars[(c + 1) * Story::LEVELS_PER_CHAPTER] > 0;
        drawSprite(p, SHARD, 6, W - 24 + c * 7, 9, got ? chapterColor(c).lighter(130) : QColor(50, 50, 70));
    }
    if (sound.isMuted())
        drawText(p, "MUTE", W - 70, 9, 1, QColor(255, 110, 110));

    // ---- info panel about the selected level
    const Story::Level &def = Story::level(mapSel);
    const int ch = Story::chapterOf(mapSel);
    const QColor acc = chapterColor(ch);
    p.fillRect(0, MAP_PANEL_Y, W, H - MAP_PANEL_Y, QColor(8, 9, 20));
    p.fillRect(0, MAP_PANEL_Y, W, 1, acc);
    drawTextCentered(p, QString::number(mapSel) + ". " + def.name, MAP_PANEL_Y + 4, 1,
                     def.boss ? QColor(255, 120, 160) : QColor(255, 220, 90));
    if (messageTimer > 0.0) {
        drawTextCentered(p, message, MAP_PANEL_Y + 14, 1, messageColor);
    } else if (mapSel > open) {
        drawTextCentered(p, "LOCKED", MAP_PANEL_Y + 14, 1, QColor(150, 150, 170));
    } else {
        // best score on the left, difficulty pips on the right
        drawText(p, QString("BEST ") + QString::number(storyBest[mapSel]), 10, MAP_PANEL_Y + 14, 1, acc);
        drawText(p, def.boss ? "FINAL QUEST" : "DIFFICULTY", 118, MAP_PANEL_Y + 14, 1, QColor(150, 160, 190));
        const int diff = 1 + (mapSel - 1) / 3;                  // 1 .. 5
        if (!def.boss)
            for (int k = 0; k < 5; ++k)
                p.fillRect(162 + k * 6, MAP_PANEL_Y + 15, 4, 3,
                           k < diff ? QColor(255, 120 + 25 * (4 - k), 60) : QColor(45, 45, 65));
    }
    drawTextCentered(p, QString("CHAPTER ") + romanNumeral(ch) + ": " + Story::chapter(ch).name,
                     MAP_PANEL_Y + 24, 1, QColor(110, 120, 160));
    drawTextCentered(p, "ARROWS: CHOOSE  ENTER: PLAY  ESC: MENU", MAP_PANEL_Y + 33, 1,
                     on ? QColor(200, 210, 230) : QColor(150, 160, 190));
}

void GameWidget::drawSceneScreen(QPainter &p)
{
    const int ch = Story::chapterOf(level);
    const QColor acc = chapterColor(ch);
    const bool on = int(animTime * 4.0) % 2 == 0;

    // ---- backdrop: night sky and the chapter's land
    p.fillRect(0, 0, W, H, acc.darker(700));
    drawStars(p, animTime);
    const int ground = 118;
    if (ch == 0) {                                     // rolling green hills
        for (int x = 0; x < W; x += 2) {
            const int h1 = 22 + int(std::sin(x * 0.045) * 8.0 + std::sin(x * 0.11) * 3.0);
            const int h2 = 12 + int(std::sin(x * 0.07 + 2.0) * 5.0);
            p.fillRect(x, ground - h1, 2, h1, QColor(30, 70, 40));
            p.fillRect(x, ground - h2, 2, h2, QColor(40, 100, 50));
        }
    } else if (ch == 1) {                              // crystal spikes
        for (int k = 0; k < 12; ++k) {
            const int cx = 8 + k * 17 + (k * 7) % 5;
            const int h = 14 + (k * 23) % 30;
            const int w = 5 + k % 3;
            for (int i = -w; i <= w; ++i) {
                const int colh = h - std::abs(i) * h / (w + 1);
                p.fillRect(cx + i, ground - colh, 1, colh, i < 0 ? QColor(60, 130, 190) : QColor(40, 90, 150));
            }
            p.fillRect(cx, ground - h + 2, 1, 2, QColor(200, 240, 255));
        }
    } else {                                           // the Shadow Spire under a storm
        const QColor tower(52, 30, 74);
        p.fillRect(W / 2 - 9, 34, 18, ground - 34, tower);
        p.fillRect(W / 2 - 13, 52, 26, 4, tower.lighter(125));
        p.fillRect(W / 2 - 5, 24, 10, 10, tower);
        p.fillRect(W / 2 - 1, 16, 2, 8, tower);
        p.fillRect(W / 2 - 9, 34, 1, ground - 34, tower.lighter(140));     // lit edge
        for (int k = 0; k < 4; ++k)
            p.fillRect(W / 2 - 2, 62 + k * 13, 4, 4,
                       (int(animTime * 3.0) + k) % 3 ? QColor(255, 70, 50) : QColor(120, 30, 30));
        if (int(animTime * 10.0) % 37 == 0)            // a lightning flash now and then
            p.fillRect(LEFT, TOP, RIGHT - LEFT, ground - TOP, QColor(255, 255, 255, 50));
    }
    p.fillRect(0, ground, W, H - ground, acc.darker(900));

    // ---- title over the art
    if (sceneIsOutro) {
        const bool shard = level % Story::LEVELS_PER_CHAPTER == 0;
        drawTextCentered(p, shard ? QString(Story::chapter(ch).shard) + " RECOVERED!" : QString("LEVEL CLEAR!"),
                         30, 2, QColor::fromHsv(int(animTime * 90) % 360, 120, 255));
    } else if (Story::level(level).boss) {
        drawTextCentered(p, "FINAL QUEST", 28, 2, on ? QColor(255, 90, 120) : QColor(255, 200, 210));
        drawTextCentered(p, Story::level(level).name, 46, 1, QColor(255, 255, 255));
    } else if ((level - 1) % Story::LEVELS_PER_CHAPTER == 0) {
        drawTextCentered(p, QString("CHAPTER ") + romanNumeral(ch), 26, 2, acc.lighter(130));
        drawTextCentered(p, Story::chapter(ch).name, 44, 1, QColor(255, 255, 255));
        drawTextCentered(p, QString("LEVEL ") + QString::number(level) + " - " + Story::level(level).name,
                         56, 1, QColor(200, 210, 230));
    } else {
        drawTextCentered(p, QString("LEVEL ") + QString::number(level), 28, 2, QColor(255, 220, 90));
        drawTextCentered(p, Story::level(level).name, 46, 1, QColor(255, 255, 255));
    }

    // ---- dialogue box
    const int bx = 6, by = 126, bw = W - 12, bh = 90;
    p.fillRect(bx, by, bw, bh, QColor(10, 12, 28));
    p.fillRect(bx, by, bw, 1, acc);
    p.fillRect(bx, by + bh - 1, bw, 1, acc.darker(200));
    p.fillRect(bx, by, 1, bh, acc.darker(150));
    p.fillRect(bx + bw - 1, by, 1, bh, acc.darker(150));

    if (!scenePages || scenePage >= sceneCount) return;
    const Story::Page &pg = scenePages[scenePage];
    int tx = bx + 8;
    if (pg.speaker != Story::NARRATOR) {
        // portrait in a frame; it bobs a little while "talking"
        const int talking = sceneChars < scenePageLength() ? int(animTime * 8.0) % 2 : 0;
        p.fillRect(bx + 5, by + 7, 28, 28, acc.darker(250));
        p.fillRect(bx + 6, by + 8, 26, 26, QColor(20, 22, 44));
        drawPortrait(p, pg.speaker, bx + 7, by + 9 - talking, 2);
        tx = bx + 40;
        drawText(p, Story::speakerName(pg.speaker), tx, by + 6, 1,
                 pg.speaker == Story::MORVAX ? QColor(220, 110, 255) : QColor(255, 220, 90));
    }
    drawText(p, QString::number(scenePage + 1) + "/" + QString::number(sceneCount),
             bx + bw - 18, by + 6, 1, QColor(90, 100, 140));

    // typewriter: only the first sceneChars characters are shown
    QString lines[8];
    const int n = wrapScenePage(lines, 8);
    int left = int(sceneChars);
    for (int i = 0; i < n && left > 0; ++i) {
        const QString shown = lines[i].left(left);
        left -= lines[i].length();
        drawText(p, shown, tx, by + 17 + i * 8, 1,
                 pg.speaker == Story::NARRATOR ? QColor(200, 210, 230) : QColor(255, 255, 255));
    }
    if (int(sceneChars) >= scenePageLength()) {        // page done: bouncing arrow
        const int bob = int(animTime * 4.0) % 2;
        drawSprite(p, DOWN_ARROW, 3, bx + bw - 10, by + bh - 7 + bob, QColor(255, 220, 90));
    }

    drawTextCentered(p, "SPACE: NEXT   ESC: SKIP", 224, 1, QColor(120, 130, 170));
}

void GameWidget::drawEndingScreen(QPainter &p)
{
    p.fillRect(0, 0, W, H, QColor(10, 10, 30));
    drawStars(p, animTime);

    // the restored Prism Heart: a rainbow crystal with turning rays
    const int cx = W / 2, cy = 66;
    for (int k = 0; k < 12; ++k) {
        const double a = animTime * 0.8 + k * 0.5236;
        for (int d = 22; d < 46; d += 4)
            p.fillRect(cx + int(std::cos(a) * d), cy + int(std::sin(a) * d), 1, 1,
                       QColor::fromHsv((k * 30 + int(animTime * 60)) % 360, 120, 255));
    }
    for (int i = 0; i < 28; ++i) {
        const int half = i < 10 ? i + 2 : (27 - i) * 12 / 17 + 1;
        const QColor c = QColor::fromHsv((i * 12 + int(animTime * 120)) % 360, 150, 255);
        p.fillRect(cx - half, cy - 14 + i, 2 * half, 1, c);
        p.fillRect(cx - half, cy - 14 + i, 1, 1, c.lighter(150));
    }
    p.fillRect(cx - 3, cy - 8, 2, 6, QColor(255, 255, 255));

    drawTextCentered(p, "THE END", 108, 3, QColor(255, 220, 90));
    drawTextCentered(p, "THE PRISM HEART SHINES AGAIN", 134, 1, QColor(255, 255, 255));
    drawTextCentered(p, "AND LUMINA IS SAVED", 144, 1, QColor(255, 255, 255));
    const QString st = QString("STARS ") + QString::number(totalStars()) + "/" + QString::number(Story::LEVELS * 3);
    drawTextCentered(p, st, 162, 2, QColor(255, 215, 60));
    if (totalStars() < Story::LEVELS * 3)
        drawTextCentered(p, "REPLAY LEVELS TO COLLECT EVERY STAR", 182, 1, QColor(150, 160, 190));
    else
        drawTextCentered(p, "PERFECT! EVERY STAR COLLECTED!", 182, 1, QColor(255, 220, 90));
    drawTextCentered(p, "THANK YOU FOR PLAYING", 198, 1, QColor(200, 210, 230));
    drawParticles(p);
    if (endLock <= 0.0)
        drawTextCentered(p, "SPACE: BACK TO THE MAP", 222, 1,
                         int(animTime * 4.0) % 2 ? QColor(255, 220, 90) : QColor(150, 160, 190));
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
