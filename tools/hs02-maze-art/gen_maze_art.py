"""Generates Hs02MazeArt.hpp for the HS-02 first-person game: sprites (drawn here as text), the
level, and a sine table. Usage: gen_maze_art.py <out.hpp>
The level is checked: the exit has to be reachable from the start."""
import math
import sys

# 16 x 16, '.' = see-through, 1..3 = the sprite's three colours
SPRITES = {
    "kBlobA": [
        "................",
        ".....111111.....",
        "...1111111111...",
        "..111111111111..",
        "..113311113311..",
        ".11132111132111.",
        ".11133111133111.",
        ".11111111111111.",
        ".11111111111111.",
        ".11112222221111.",
        ".11111222211111.",
        "..111111111111..",
        "..111111111111..",
        ".1111.1111.1111.",
        ".111...11...111.",
        "..1.....1....1..",
    ],
    "kBlobB": [
        "................",
        "................",
        ".....111111.....",
        "...1111111111...",
        "..113311113311..",
        ".11132111132111.",
        ".11133111133111.",
        ".11111111111111.",
        ".11112222221111.",
        ".11122222222111.",
        ".11111222211111.",
        "..111111111111..",
        ".11111111111111.",
        ".111.1111.11111.",
        "..11..111..111..",
        "...1...1....1...",
    ],
    "kKit": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "...1111111111...",
        "..111111111111..",
        "..111112211111..",
        "..111112211111..",
        "..111222222111..",
        "..111222222111..",
        "..111112211111..",
        "..111112211111..",
        "..111111111111..",
        "...1111111111...",
    ],
    # the player's tool, seen from behind: a soldering iron pointing into the picture
    "kIron": [
        "................",
        "................",
        "................",
        ".......22.......",
        ".......22.......",
        "......2222......",
        "......2222......",
        "......1111......",
        "......1111......",
        ".....111111.....",
        ".....111111.....",
        ".....333333.....",
        "....33333333....",
        "....33333333....",
        "...3333333333...",
        "...3333333333...",
    ],
    "kIronHot": [
        "................",
        ".......33.......",
        "......3223......",
        ".....322223.....",
        "......3223......",
        "......2222......",
        "......2222......",
        "......1111......",
        "......1111......",
        ".....111111.....",
        ".....111111.....",
        ".....333333.....",
        "....33333333....",
        "....33333333....",
        "...3333333333...",
        "...3333333333...",
    ],
}

# '#' wall, 'P' start (facing east), 'E' exit (a wall you walk into), 'b' enemy, 'm' repair kit
LEVEL = [
    "################",
    "#P...#....#...b#",
    "#.##.#.##.#.##.#",
    "#.#..#.#..#..#.#",
    "#.#.##.#.###.#.#",
    "#.#....#...#...#",
    "#.####.###.#.###",
    "#....#...#.#...#",
    "####.#.#.#.###.#",
    "#b...#.#.#...#.#",
    "#.####.#.###.#.#",
    "#.#m...#...#.#.#",
    "#.#.######.#.#.#",
    "#.#..b...#m#..b#",
    "#...##.#.....#E#",
    "################",
]


def check_level():
    assert len(LEVEL) == 16 and all(len(r) == 16 for r in LEVEL)
    start = [(x, y) for y, r in enumerate(LEVEL) for x, c in enumerate(r) if c == "P"]
    exits = [(x, y) for y, r in enumerate(LEVEL) for x, c in enumerate(r) if c == "E"]
    assert len(start) == 1 and len(exits) == 1
    seen, todo = {start[0]}, [start[0]]
    while todo:
        x, y = todo.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n not in seen and LEVEL[n[1]][n[0]] not in "#E":
                seen.add(n)
                todo.append(n)
    ex, ey = exits[0]
    assert any((ex + dx, ey + dy) in seen for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))), "exit not reachable"
    for y, r in enumerate(LEVEL):
        for x, c in enumerate(r):
            if c in "bm":
                assert (x, y) in seen, ("thing not reachable", x, y)
    things = sum(r.count("b") + r.count("m") for r in LEVEL)
    return len(seen), things


def pack(rows):
    assert len(rows) == 16 and all(len(r) == 16 for r in rows), rows
    out = []
    for r in rows:
        for b in range(4):  # four pixels per byte, first pixel in the low bits
            v = 0
            for i in range(4):
                c = r[b * 4 + i]
                v |= (0 if c == "." else int(c)) << (i * 2)
            out.append(v)
    return out


cells, things = check_level()
with open(sys.argv[1], "w", encoding="utf-8", newline="\n") as f:
    f.write("#pragma once\n#include <stdint.h>\n\n")
    f.write("// Art and level of the HS-02 first-person game (Hs02Maze.cpp). Generated; the drawings and the\n")
    f.write("// level were made for this firmware.\n")
    f.write("namespace Hs02MazeArt {\n")
    f.write("// Sprites: 16 x 16 pixels, two bits per pixel (0 = see-through, 1 to 3 = the sprite's own three\n")
    f.write("// colours), four pixels per byte with the first pixel in the low bits, rows top to bottom.\n")
    for name, rows in SPRITES.items():
        f.write("constexpr uint8_t %s[64] = {%s};\n" % (name, ", ".join("0x%02X" % v for v in pack(rows))))
    f.write("\n// The level: '#' wall, 'P' start (facing east), 'E' exit (a wall the player walks into),\n")
    f.write("// 'b' enemy, 'm' repair kit, '.' floor. %d cells can be reached; %d things.\n" % (cells, things))
    f.write("constexpr uint8_t kThingCount = %d;\n" % things)
    f.write("constexpr char    kLevel[16][17] = {\n")
    for r in LEVEL:
        f.write('    "%s",\n' % r)
    f.write("};\n\n// sin(i * 90 degrees / 64) * 256, for i = 0..64\n")
    f.write("constexpr int16_t kSine[65] = {%s};\n" % ", ".join(str(round(256 * math.sin(i * math.pi / 128))) for i in range(65)))
    f.write("} // namespace Hs02MazeArt\n")
print("wrote", sys.argv[1], "| reachable cells:", cells, "| things:", things)
