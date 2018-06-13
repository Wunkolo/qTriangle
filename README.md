# qTriangle [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/Wunkolo/qTriangle/master/LICENSE)

qTriangle is a personal study to design a **q**uick way to rasterize **Triangles** by optimizing the and vectorizing the Point-In-Triangle problem.

The domain of the Point-In-Triangle problem is determining if a cartesian coordinate happens to land upon the interior of a triangle. In this case the 2D case of triangles will be examined and will require some kind of surface **area** for a point to land on so a case in which all three points are collinear(which is the worst case of having a *very* slim triangle) are ruled out.

This problem comes up a lot in the domain of computer graphics and game-play programming at times. Sometimes it's testing if a single point lands upon a polygon(made up of triangles) and sometimes it's testing if thousands of points happen to land on a triangle or not(such as when rendering a vector triangle against a regular grid during rasterization).

There are two primary formula to test if a point happens to land within a triangle in 2D space.

# Cross Product Method

The cross-product operation in vector algebra is a binary operation that takes two vectors in 3D space and creates a new vector that is perpendicular to them both. You can think of the two vectors as describing some kind of plane in 2D space, and the cross product creates a new vector that is perpendicular to this plane. Though, there are two ways to create a vector perpendicular to a plane, by going "in" the plane and going "out" the plane.

The Cross-Product has a lot of useful properties but one of interest at the moment is the **magnitude** of the resulting vector of the cross product which will be the **area** of the parallelogram that the two original vectors create. Since the points are on the X-Y plane in 3D space this *magnitude* will always be the Z-component of the cross product since all vectors perpendicular to the X-Y plane will take the form `[ 0, 0, (some value)]`. This becomes useful later on.

![](media/Cross.gif)

The particular numerical value of this area is not of importance either but rather the *parity* of the area is of interest(while a negative-surface-area does not make sense, this value tells us something about two directional vectors). Notice that whenever the direction to the moving point goes to the "left" of the black directional vector, that the area becomes negative, but when it is to the "right", the area is positive.
This is due to the [right hand rule](https://en.wikipedia.org/wiki/Right-hand_rule) where the direction of positive-rotation being **clockwise** or **counter-clockwise** causes the order of the original two directional vectors to determine the proper orientation of the cross product.

The parity of the cross-product-area depending on which side the direction of the "point" lands on solves the problem at the same tone of turning each edge into a linear inequality then testing if the point solves each of them at once but in a somewhat more optimal way.

The three positional vectors of the triangle must be in **clockwise** or **counter-clockwise** order so that three directional vectors can be determinately created.
```
EdgeDir0 = Vertex1 - Vertex0
EdgeDir1 = Vertex2 - Vertex1
EdgeDir2 = Vertex0 - Vertex2
```
Then, three additional directional vectors can be made that point from the triangle vertex position to the point that is being tested against
```
PointDir0 = Point - Vertex0
PointDir1 = Point - Vertex1
PointDir2 = Point - Vertex2
```

Now, finding out if a point lands within the triangle is determined by using three cross-products, and checking if each area is positive. If they are all positive. Then the point is to the "right" of all the edges. If any of them are negative, then it is not within the triangle.
```
| EdgeDir0 × PointDir0 | >= 0 &&
| EdgeDir1 × PointDir1 | >= 0 &&
| EdgeDir2 × PointDir2 | >= 0
```

![](media/CrossMethod.gif)

## Optimizations

Previously it was determined that all cross products against the X-Y plane will take the form `[ 0, 0, (some value)]`. With this the arithmetic behind the cross-product operation can be much more simplified. Since the cross product can be [calculated using partial determinants of a 3x3 matrix](https://en.wikipedia.org/wiki/Rule_of_Sarrus) then attention only has to be given to the calculations that determine the Z-component alone which is but a 2x2 determinant of the two input vectors.

This means that if I had two vectors `A` and `B` on the X-Y plane. The cross-product's magnitude is simply:
```
A.x * B.y - A.y * B.x;
```
which reduces the previous arithmetic to:
```
EdgeDir0.x * PointDir0.y - EdgeDir0.y * PointDir0.x >= 0 &&
EdgeDir1.x * PointDir1.y - EdgeDir1.y * PointDir1.x >= 0 &&
EdgeDir2.x * PointDir2.y - EdgeDir2.y * PointDir2.x >= 0
```

The full pseudo-code:
```cpp
// Point       - Position that is being tested
// Vertex0,1,2 - Vertices of the triangle in clockwise order

// Directional vertices along the edges of the triangle in clockwise order
EdgeDir0 = Vertex1 - Vertex0
EdgeDir1 = Vertex2 - Vertex1
EdgeDir2 = Vertex0 - Vertex2

// Directional vertices pointing from the triangle vertices to the point
PointDir0 = Point - Vertex0
PointDir1 = Point - Vertex1
PointDir2 = Point - Vertex2

// Test if each cross-product results in a positive area
if(
	EdgeDir0.x * PointDir0.y - EdgeDir0.y * PointDir0.x >= 0 &&
	EdgeDir1.x * PointDir1.y - EdgeDir1.y * PointDir1.x >= 0 &&
	EdgeDir2.x * PointDir2.y - EdgeDir2.y * PointDir2.x >= 0
)
{
	// CurPoint is in triangle!
}
```

## Scaling

If I was to throw thousands of points at a triangle in a for-loop using this algorithm then not all variables have to be re-calculated.

The vectors `EdgeDir0`, `EdgeDir1`, `EdgeDir2` only have to be calculated once. For each point the vectors `PointDir0`, `PointDir1`, `PointDir2` have to be recreated.

```
EdgeDir0 = Vertex1 - Vertex0
EdgeDir1 = Vertex2 - Vertex1
EdgeDir2 = Vertex0 - Vertex2
foreach(CurPoint in LotsOfPoints)
{
	PointDir0 = Point - Vertex0
	PointDir1 = Point - Vertex1
	PointDir2 = Point - Vertex2
	if(
		EdgeDir0.x * PointDir0.y - EdgeDir0.y * PointDir0.x >= 0 &&
		EdgeDir1.x * PointDir1.y - EdgeDir1.y * PointDir1.x >= 0 &&
		EdgeDir2.x * PointDir2.y - EdgeDir2.y * PointDir2.x >= 0
	)
	{
		// CurPoint is in triangle!
	}
}
```
Which results in the total overhead for each point being
Subtractions|Multiplications|Comparisons
:-:|:-:|:-:
9|6|3

# Barycentric Coordinate Method

![](media/BarycentricMethod.gif)