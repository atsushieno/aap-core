import png

r = png.Reader('GroupsAndLabels.png')
data = r.read()
width = data[0]
height = data[1]
pixels = list(data[2])
newPixels = list()
for row in pixels:
    newRow = list()
    for col in xrange(width):
        alpha = row[4 * col]
        newRow.append(255)
        newRow.append(255)
        newRow.append(255)
        newRow.append(alpha)
    newPixels.append(newRow)

outfile = open('GroupsAndLabelsMask.png', 'wb')
w = png.Writer(width, height, alpha=True)
w.write(outfile, newPixels)
        
outfile.close()
