import os
from subprocess import call
import numpy as np
import itertools
from PIL import Image
from PIL import ImageDraw

TestTriangle = (
	( 5, 5),
	(95,40),
	(30,95)
)

# Barycentric method
def PointInTriangle( Point, Triangle ):
	V0 = tuple(np.subtract(Triangle[2], Triangle[0]))
	V1 = tuple(np.subtract(Triangle[1], Triangle[0]))
	V2 = tuple(np.subtract(Point      , Triangle[0]))

	Dot00 = np.dot(V0, V0)
	Dot01 = np.dot(V0, V1)
	Dot02 = np.dot(V0, V2)
	Dot11 = np.dot(V1, V1)
	Dot12 = np.dot(V1, V2)
	Area  = (Dot00 * Dot11 - Dot01 * Dot01)
	U     = (Dot11 * Dot02 - Dot01 * Dot12)
	V     = (Dot00 * Dot12 - Dot01 * Dot02)
	return (U >= 0) & (V >= 0) & (U + V < Area)

def chunks(List, Widths):
	i = 0
	for CurWidth in Widths:
		while i + CurWidth < len(List):
			yield List[i:i + CurWidth]
			i += CurWidth

# Params:
# Name: Name of generated frames. Default "Serial"
# Path: Path for output images Default: "./frames/(Name)/"
# Size: Of the image, default (100,100)
# Scale: Scaling for the resulting image. Default: 2
# Granularity: List of widths in which elements may be processed in parallel
#              Default: [1]
def RenderTriangle( Params ):
	# Params
	Name = Params.get("Name", "Serial")
	Size = Params.get("Size", (100, 100))
	Path = "./frames/" + Name + "/"
	Scale = Params.get("Scale", 2)
	Granularity = Params.get("Granularity", [1])
	# Sort by largest to smallest
	Granularity.sort()
	Granularity.reverse()
	# Create target path recursively
	os.makedirs(Path, exist_ok=True)
	# Create image
	Img = Image.new('RGB', Size)
	Draw = ImageDraw.Draw(Img)
	# Generate each row of points up-front
	Points = [
		[
			(x,y) for x in range(Size[0]) if PointInTriangle((x,y),TestTriangle)
		]
		for y in range(Size[1])
	]
	i = 0
	for Row in Points:
		for CurPoints in chunks(Row,Granularity):
			# Hilight the pixels being currently processed
			Draw.point(
				CurPoints,
				fill=(0xFF, 0x00, 0x00)
			)
			Img.resize(
				(Img.width * Scale, Img.height * Scale),
				Image.NEAREST
			).save(Path + Name + '_' + str(i).zfill(6) + ".png")
			i += 1
			# Save the "processed" frame
			Draw.point(
				CurPoints,
				fill=(0xFF, 0xFF, 0xFF)
			)
			Img.resize(
				(Img.width * Scale, Img.height * Scale),
				Image.NEAREST
			).save(Path + Name + '_' + str(i).zfill(6) + ".png")
			i += 1
			
	call(
		[
			'ffmpeg',
			'-framerate','50',
			'-i', Path + Name + '_%6d.png',
			Name + '.gif',
			'-t','4'
		]
	)


# Serial
RenderTriangle(
	{
		"Name": "Serial",
		"Granularity": [1],
		"Scale": 2,
		"Size": (100, 100)
	}
)

# SSE/NEON
RenderTriangle(
	{
		"Name": "SSE-NEON",
		"Granularity": [4,1],
		"Scale": 2,
		"Size": (100, 100)
	}
)

# AVX2
RenderTriangle(
	{
		"Name": "AVX2",
		"Granularity": [8,4,1],
		"Scale": 2,
		"Size": (100, 100)
	}
)

# AVX512
RenderTriangle(
	{
		"Name": "AVX512",
		"Granularity": [16,8,4,1],
		"Scale": 2,
		"Size": (100, 100)
	}
)

# ffmpeg -f image2 -framerate 50 -i Serial-%6d.png Serial.gif -t 4

