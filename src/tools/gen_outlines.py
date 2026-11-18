import json
import os
import math

temp = os.environ.get("TEMP", ".")
with open(os.path.join(temp, "land50.json"), "r", encoding="utf-8") as f:
    data = json.load(f)


def ring_area(ring):
    a = 0.0
    n = len(ring)
    for i in range(n):
        x1, y1 = ring[i]
        x2, y2 = ring[(i + 1) % n]
        a += x1 * y2 - x2 * y1
    return abs(a) / 2.0


def perp_distance(pt, a, b):
    (x, y), (ax, ay), (bx, by) = pt, a, b
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(x - ax, y - ay)
    t = ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    px, py = ax + t * dx, ay + t * dy
    return math.hypot(x - px, y - py)


def rdp(points, epsilon):
    if len(points) < 3:
        return points
    dmax = 0.0
    index = 0
    for i in range(1, len(points) - 1):
        d = perp_distance(points[i], points[0], points[-1])
        if d > dmax:
            index = i
            dmax = d
    if dmax > epsilon:
        left = rdp(points[:index + 1], epsilon)
        right = rdp(points[index:], epsilon)
        return left[:-1] + right
    else:
        return [points[0], points[-1]]


def simplify_closed(ring, epsilon):
    pts = ring[:]
    if pts[0] == pts[-1]:
        pts = pts[:-1]
    n = len(pts)
    if n < 4:
        return pts
    far_idx = max(range(1, n), key=lambda i: math.hypot(pts[i][0]-pts[0][0], pts[i][1]-pts[0][1]))
    part1 = pts[0:far_idx+1]
    part2 = pts[far_idx:] + [pts[0]]
    s1 = rdp(part1, epsilon)
    s2 = rdp(part2, epsilon)
    result = s1[:-1] + s2[:-1]
    return result


polys = []
for feat in data["features"]:
    geom = feat["geometry"]
    if geom["type"] == "Polygon":
        rings = [geom["coordinates"][0]]
    elif geom["type"] == "MultiPolygon":
        rings = [poly[0] for poly in geom["coordinates"]]
    else:
        continue
    for ring in rings:
        area = ring_area(ring)
        polys.append((area, ring))

polys.sort(key=lambda p: -p[0])

MIN_AREA = 0.5
results = []
for area, ring in polys:
    if area < MIN_AREA:
        continue
    if area > 1000:
        eps = 0.35
    elif area > 100:
        eps = 0.15
    elif area > 10:
        eps = 0.08
    else:
        eps = 0.04
    simplified = simplify_closed(ring, eps)
    results.append((area, simplified))

total_pts = sum(len(r) for _, r in results)
print("kept polygons:", len(results), "total points:", total_pts)

out_path = os.path.join(os.path.dirname(__file__), "..", "src", "app", "ContinentOutlinesDetailed.inc")
with open(out_path, "w", encoding="utf-8") as f:
    f.write("// Auto-generated from Natural Earth 1:50m land polygons\n")
    f.write("// Source: https://www.naturalearthdata.com/downloads/50m-physical-vectors/50m-land/\n")
    f.write("// Simplified with the Douglas-Peucker algorithm to keep a reasonable point count.\n")
    f.write("// Coordinates are (longitude, latitude) in degrees.\n")
    f.write("static const QVector<QVector<QPointF>> kDetailedOutlines = {\n")
    for area, ring in results:
        f.write("    {\n")
        for i in range(0, len(ring), 6):
            chunk = ring[i:i+6]
            items = ", ".join(f"{{{lon:.2f}, {lat:.2f}}}" for lon, lat in chunk)
            f.write("        " + items + ",\n")
        f.write("    },\n")
    f.write("};\n")

print("wrote", out_path)
