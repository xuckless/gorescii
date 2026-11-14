# Map format

Plain text, one statement per line, `#` starts a comment. Units are metres;
+x is east, +y is north, angles are degrees counter-clockwise from east.

```
name <title>
sector [key=value ...] [exit] : x,y x,y x,y ...
thing <type> x,y [angle]
```

**Sectors** are convex polygons. Corners may be listed in either winding;
collinear corners are fine. Two sectors are joined by a portal when they share
an edge with *identical* corners, so if a doorway meets a long wall, add the
doorway's corners to the wall's sector too. The loader rejects concave
sectors and edges that overlap a neighbour without matching corners.

| key        | default | meaning                                  |
|------------|---------|------------------------------------------|
| `floor`    | 0       | floor height                             |
| `ceil`     | 3       | ceiling height                           |
| `light`    | 0.8     | brightness 0..1                          |
| `wall`     | brick   | wall texture                             |
| `floortex` | tile    | floor texture                            |
| `ceiltex`  | panel   | ceiling texture; `sky` opens it to sky   |
| `damage`   | 0       | health lost per second on the floor      |
| `exit`     |         | flag: standing here finishes the level   |
| `door`     |         | flag: starts shut, opens to `ceil` on use|
| `key`      |         | `red` or `blue`: door needs that key     |
| `lift`     |         | flag: lowers to `low` on use, then rises |
| `low`      |         | lift's bottom height                     |

Textures: `brick metal stone tech wood tile panel dirt slime sky`.

The player can step up 0.6 and needs 1.9 of headroom.

Doors and lifts are used with E, facing them from up to 1.6 away (or
standing on a lift). Give a door its own `wall` texture: its face is drawn
with it.

**Things**: `player` (exactly one), `grunt`, `brute`, `medkit`, `ammo`,
`armor`, `redkey`, `bluekey`.
