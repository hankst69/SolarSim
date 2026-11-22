import re

with open("ContinentOutlinesDetailed.inc", "r") as f:
    content = f.read()

start = content.index("kDetailedOutlines = {") + len("kDetailedOutlines = {")
end = content.rindex("};")
body = content[start:end]

polygons = []
idx = 0
n = len(body)
while idx < n:
    c = body[idx]
    if c == '{':
        d = 1
        j = idx + 1
        while d > 0:
            if body[j] == '{':
                d += 1
            elif body[j] == '}':
                d -= 1
            j += 1
        block = body[idx + 1:j - 1]
        polygons.append(block)
        idx = j
    else:
        idx += 1

print("num polygons:", len(polygons))

flat = []
counts = []
for poly in polygons:
    pts = re.findall(r'\{\s*(-?\d+\.?\d*)\s*,\s*(-?\d+\.?\d*)\s*\}', poly)
    counts.append(len(pts))
    for x, y in pts:
        flat.append(x)
        flat.append(y)

print("total points:", sum(counts))
print("total flat values:", len(flat))

with open("ContinentOutlinesDetailedData.inc", "w") as f:
    f.write("// Auto-generated flattened coordinate data for detailed continent outlines.\n")
    f.write("// kDetailedOutlinesCounts holds the number of points in each polygon; \n")
    f.write("// kDetailedOutlinesCoords holds the flattened (lon, lat) pairs for all \n")
    f.write("// polygons concatenated in order. This flat/raw array representation compiles\n")
    f.write("// far faster than a nested QVector<QVector<QPointF>> initializer list with\n")
    f.write("// thousands of QPointF constructor calls.\n")
    f.write("static const int kDetailedOutlinesCounts[] = {\n")
    for i in range(0, len(counts), 20):
        f.write("    " + ", ".join(str(c) for c in counts[i:i + 20]) + ",\n")
    f.write("};\n\n")
    f.write("static const double kDetailedOutlinesCoords[] = {\n")
    for i in range(0, len(flat), 10):
        f.write("    " + ", ".join(flat[i:i + 10]) + ",\n")
    f.write("};\n")

print("done")
