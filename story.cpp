#include "story.h"

namespace Story {

namespace {

const Chapter CHAPTER_LIST[CHAPTERS] = {
    { "SUNLIT MEADOW",   "SUN SHARD",  0x6ad06a },
    { "CRYSTAL CAVERNS", "MOON SHARD", 0x5ac8ff },
    { "SHADOW SPIRE",    "STAR SHARD", 0xc060ff }
};

// Difficulty rises slowly: faster ball, narrower paddle, fewer helpful
// capsules and more curses. New brick types are introduced one at a time.
const Level LEVEL_LIST[LEVELS] = {
    // ------------------------------------------------------------------
    // CHAPTER I - SUNLIT MEADOW
    // ------------------------------------------------------------------
    { "FIRST LIGHT",
      { "............",
        "............",
        "...111111...",
        "..11111111..",
        ".1111111111.",
        "..11111111..",
        "............",
        "............" },
      70.0, 40.0, 26, 0, false,
      { { NARRATOR, "LONG AGO THE PRISM HEART FILLED THE KINGDOM OF LUMINA WITH LIGHT. "
                    "ITS GLOW KEPT THE FIELDS GREEN AND THE NIGHTS SAFE." },
        { NARRATOR, "THEN MORVAX THE SHADOW WEAVER SHATTERED IT, AND LOCKED THE LIGHT AWAY "
                    "INSIDE WALLS OF ENCHANTED BRICK." },
        { ELDER,    "WAKE UP, WARDEN! YOU ARE THE LAST ONE WHO CAN WIELD THE SPARK ORB. "
                    "BREAK THE WALLS AND SET THE LIGHT FREE!" } },
      {} },

    { "MEADOW PATH",
      { "............",
        "11111..11111",
        "12221..12221",
        "11111..11111",
        "............",
        "..11111111..",
        "..12222221..",
        "............" },
      74.0, 38.0, 25, 0, false,
      { { PIP, "HI! I'M PIP, A LITTLE SPARK YOU FREED BACK THERE. "
               "I'LL TAG ALONG - THIS MEADOW IS FULL OF TRAPPED LIGHT!" },
        { PIP, "ORANGE BRICKS ARE TOUGHER. YOU HAVE TO HIT THEM TWICE." } },
      {} },

    { "WILDFLOWER HILL",
      { "....2H22....",
        "...211112...",
        "..21111112..",
        ".2111111112.",
        "211111111112",
        ".2111111112.",
        "..21111112..",
        "...222222..." },
      78.0, 36.0, 24, 15, false,
      { { MORVAX, "WHO DARES TOUCH MY WALLS? A CHILD WITH A PADDLE? HOW AMUSING." },
        { PIP,    "SEE THE RED CAPSULES WITH A SKULL? THOSE ARE HIS CURSES. "
                  "DODGE THEM - DON'T CATCH THEM!" } },
      {} },

    { "POWDER KEGS",
      { "............",
        "111111111111",
        "1B111BB111B1",
        "111111111111",
        "............",
        "222222222222",
        "22B222222B22",
        "............" },
      82.0, 36.0, 22, 20, false,
      { { ELDER, "CAREFUL! THE DARK BRICKS WITH A FUSE ARE FIRE-POWDER. "
                 "ONE HIT AND THEY BLOW UP EVERYTHING AROUND THEM." },
        { PIP,   "SO... WE WANT TO HIT THEM. A LOT. RIGHT?" } },
      {} },

    { "THE SUN GATE",
      { "..33333333..",
        ".2222222222.",
        "21111BB11112",
        "211#1111#112",
        "211111111112",
        ".2222222222.",
        "..22222222..",
        "............" },
      86.0, 36.0, 22, 20, false,
      { { PIP,   "I CAN FEEL IT... THE FIRST PRISM SHARD IS BEHIND THIS GATE!" },
        { ELDER, "THE GRAY STONES CANNOT BE BROKEN. USE THEM TO BOUNCE YOUR ORB." } },
      { { NARRATOR, "THE GATE CRUMBLES. A WARM GOLDEN LIGHT RISES INTO THE SKY: "
                    "THE SUN SHARD IS FREE!" },
        { ELDER,    "THE MEADOW BREATHES AGAIN. BUT THE NEXT SHARD LIES DEEP "
                    "BENEATH THE MOUNTAINS, IN THE CRYSTAL CAVERNS." } } },

    // ------------------------------------------------------------------
    // CHAPTER II - CRYSTAL CAVERNS
    // ------------------------------------------------------------------
    { "FROZEN DESCENT",
      { "1.1.1.1.1.1.",
        ".1.1.1.1.1.1",
        "2.2.2.2.2.2.",
        ".2.2.2.2.2.2",
        "3.3.3.3.3.3.",
        ".3.3.3.3.3.3",
        "............",
        "............" },
      90.0, 34.0, 20, 25, false,
      { { NARRATOR, "BENEATH THE MOUNTAINS THE CRYSTAL CAVERNS GLITTER, COLD AND SILENT." },
        { MORVAX,   "YOU FOUND ONE SHARD. HOW BRAVE. YOU WILL NEVER LEAVE MY CAVERNS." } },
      {} },

    { "GHOST LIGHTS",
      { "............",
        "GGGG....GGGG",
        "G22G....G22G",
        "GGGG....GGGG",
        "....GGGG....",
        "....G22G....",
        "....GGGG....",
        "111111111111" },
      94.0, 34.0, 20, 25, false,
      { { PIP, "WHOA, THESE PALE BRICKS FLICKER IN AND OUT! YOUR ORB "
               "PASSES RIGHT THROUGH THEM WHILE THEY ARE FADED." },
        { PIP, "WAIT UNTIL THEY ARE SOLID, THEN STRIKE!" } },
      {} },

    { "CRYSTAL TEETH",
      { "#3........3#",
        "#33......33#",
        "#333....333#",
        "#2B222222B2#",
        "#2222222222#",
        "..2.2..2.2..",
        "..1.1..1.1..",
        "............" },
      98.0, 34.0, 19, 30, false,
      { { ELDER, "MORVAX'S CURSES GROW STRONGER DOWN HERE. SOME EVEN BRING DARKNESS. "
                 "WATCH EVERY FALLING CAPSULE." } },
      {} },

    { "MIRROR HALLS",
      { "GGGGGGGGGGGG",
        "3.3.3..3.3.3",
        "BGGGGGGGGGGB",
        "3.3.3..3.3.3",
        "GGGGGGGGGGGG",
        "222222222222",
        "2#2#2..2#2#2",
        "............" },
      102.0, 34.0, 19, 30, false,
      { { PIP, "THE WALLS ARE GETTING THICKER. IF YOU SEE A FIREBALL, GRAB IT - "
               "IT BURNS RIGHT THROUGH BRICKS!" } },
      {} },

    { "THE MOON GATE",
      { "H#3333333333",
        "#33322223333",
        "333221122333",
        "322111111223",
        "332B1111B233",
        "333222222333",
        "#2222222222#",
        "............" },
      106.0, 34.0, 18, 30, false,
      { { PIP,   "THE MOON SHARD IS SINGING... CAN YOU HEAR IT? IT'S RIGHT HERE!" },
        { ELDER, "A HEART IS SEALED IN THE CORNER. ONLY A SHOT FROM ABOVE CAN REACH IT." } },
      { { NARRATOR, "SILVER LIGHT FLOODS THE CAVERNS. THE MOON SHARD IS FREE!" },
        { ELDER,    "TWO SHARDS SHINE AGAIN. ONLY THE STAR SHARD REMAINS - "
                    "AND MORVAX KEEPS IT AT THE TOP OF HIS SHADOW SPIRE." },
        { MORVAX,   "COME THEN, LITTLE WARDEN. CLIMB MY SPIRE. I WILL BE WAITING." } } },

    // ------------------------------------------------------------------
    // CHAPTER III - SHADOW SPIRE
    // ------------------------------------------------------------------
    { "OBSIDIAN STAIRS",
      { "44........44",
        "3344....4433",
        "223344443322",
        "112233332211",
        "..11222211..",
        "....1111....",
        "............",
        "............" },
      110.0, 32.0, 17, 35, false,
      { { NARRATOR, "A BLACK TOWER PIERCES THE STORM CLOUDS. EVERY FLOOR IS GUARDED "
                    "BY WALLS OF OBSIDIAN." },
        { PIP,      "PURPLE BRICKS TAKE FOUR HITS. DEEP BREATHS, WARDEN." } },
      {} },

    { "STORMWALL",
      { "333333333333",
        "##.######.##",
        "2G2G2G2G2G2G",
        "G2G2G2G2G2G2",
        "..##....##..",
        "1B11111111B1",
        "111111111111",
        "............" },
      114.0, 32.0, 17, 35, false,
      { { MORVAX, "TURN BACK. THE DARK IS PATIENT, AND YOUR LIGHT IS SO VERY SMALL." },
        { PIP,    "THE ONLY WAY UP IS THROUGH THOSE NARROW GAPS. AIM CAREFULLY!" } },
      {} },

    { "THE LONG DARK",
      { "4.4.4.4.4.4.",
        ".3.3.3.3.3.3",
        "G.G.G.G.G.G.",
        ".G.G.G.G.G.G",
        "2B22222222B2",
        "222222222222",
        "#..#.##.#..#",
        "............" },
      118.0, 32.0, 16, 40, false,
      { { ELDER, "DO NOT LISTEN TO HIM, WARDEN. EVERY SPARK OF LIGHT YOU FREED "
                 "STANDS BEHIND YOU NOW." } },
      {} },

    { "THE LAST GATE",
      { "H#44444444#H",
        "##33333333##",
        "3B33333333B3",
        "222GGGGGG222",
        "222222222222",
        "1#1#1..1#1#1",
        "111111111111",
        "............" },
      122.0, 32.0, 16, 40, false,
      { { PIP, "THE LAST GATE BEFORE THE TOP... I'M SCARED. BUT I'M RIGHT HERE WITH YOU." } },
      {} },

    { "CROWN OF THE SPIRE",
      { "............",
        "............",
        "............",
        "#.11111111.#",
        "..2.2..2.2..",
        "............",
        "............",
        "............" },
      104.0, 34.0, 20, 25, true,
      { { NARRATOR, "THE FINAL QUEST. AT THE TOP OF THE SPIRE, WRAPPED IN A STORM OF "
                    "SHADOW, MORVAX HOLDS THE STAR SHARD." },
        { MORVAX,   "SO, THE LITTLE WARDEN ARRIVES AT LAST. LET US SEE HOW YOUR LIGHT "
                    "FARES AGAINST ME!" },
        { PIP,      "HIT HIM WITH THE ORB! AND DODGE HIS SHADOW BOLTS - "
                    "THEY FREEZE YOUR PADDLE!" } },
      { { MORVAX,   "NO... THE LIGHT... IT BURNS... THIS CANNOT BE..." },
        { NARRATOR, "THE STAR SHARD FALLS FREE. THE THREE SHARDS JOIN, AND THE PRISM "
                    "HEART BLAZES ONCE MORE OVER ALL OF LUMINA." },
        { ELDER,    "YOU DID IT, WARDEN. THE KINGDOM WILL SING YOUR NAME FOR A THOUSAND YEARS." } } }
};

} // namespace

const Level &level(int n)
{
    if (n < 1) n = 1;
    if (n > LEVELS) n = LEVELS;
    return LEVEL_LIST[n - 1];
}

const Chapter &chapter(int c)
{
    if (c < 0) c = 0;
    if (c >= CHAPTERS) c = CHAPTERS - 1;
    return CHAPTER_LIST[c];
}

int chapterOf(int n)
{
    if (n < 1) n = 1;
    if (n > LEVELS) n = LEVELS;
    return (n - 1) / LEVELS_PER_CHAPTER;
}

const char *speakerName(int s)
{
    switch (s) {
    case ELDER:  return "ELDER OWYN";
    case PIP:    return "PIP";
    case MORVAX: return "MORVAX";
    default:     return "";
    }
}

int pageCount(const Page *pages)
{
    int n = 0;
    while (n < MAX_PAGES && pages[n].text) ++n;
    return n;
}

} // namespace Story
