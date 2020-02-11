import png

r = png.Reader('Sarah.png')
data = r.read()
width = data[0]
height = data[1]
pixels = list(data[2])
newPixels = list()
for row in pixels:
    newRow = list()
    for col in xrange(width):
        alpha = row[4 * col + 2]    # blue component
        newRow.append(68)
        newRow.append(114)
        newRow.append(196)
        newRow.append(alpha)
    newPixels.append(newRow)

outfile = open('SarahMask.png', 'wb')
w = png.Writer(width, height, alpha=True)
w.write(outfile, newPixels)
        
outfile.close()
