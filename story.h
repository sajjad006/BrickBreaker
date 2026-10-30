#ifndef STORY_H
#define STORY_H

// Story mode data: the levels of the campaign (layouts and difficulty), the
// dialogue shown before / after them and the chapters they belong to.
//
// THE TALE OF LUMINA
//   Morvax the Shadow Weaver shattered the Prism Heart and sealed its light
//   inside walls of enchanted brick. The Warden (the player) frees the light
//   level by level, recovers the Sun, Moon and Star shards at the end of each
//   chapter, and finally faces Morvax at the top of his Shadow Spire.
namespace Story {

enum {
    LEVELS             = 15,
    CHAPTERS           = 3,
    LEVELS_PER_CHAPTER = 5,
    MAX_PAGES          = 3
};

enum Speaker { NARRATOR = 0, ELDER, PIP, MORVAX };

struct Page { int speaker; const char *text; };

// Layout characters, 12 per row:
//   '.' empty   '1'..'4' brick that needs that many hits   '#' solid (unbreakable)
//   'H' heart brick (+1 life)   'B' bomb brick (blows up its neighbours)
//   'G' ghost brick (fades in and out; the ball passes through it while faded)
struct Level {
    const char *name;
    const char *rows[8];
    double speed;               // ball speed (pixels per second) at the start
    double paddle;              // paddle width in pixels
    int    dropPct;             // chance (percent) that a broken brick drops a capsule
    int    cursePct;            // share (percent) of the capsules that are curses
    bool   boss;                // the final quest: Morvax fights back
    Page   intro[MAX_PAGES];    // shown before the level (a null text ends the list)
    Page   outro[MAX_PAGES];    // shown after the level is cleared
};

struct Chapter { const char *name; const char *shard; unsigned color; };

const Level   &level(int n);            // n = 1 .. LEVELS
const Chapter &chapter(int c);          // c = 0 .. CHAPTERS - 1
int            chapterOf(int n);        // chapter index of level n
const char    *speakerName(int s);
int            pageCount(const Page *pages);

} // namespace Story

#endif // STORY_H
