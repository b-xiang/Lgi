#ifndef _GPOINT_H_
#define _GPOINT_H_

/// 2d Point
class LgiClass GdcPt2
{
public:
	int x, y;

	GdcPt2(int Ix = 0, int Iy = 0)
	{
		x = Ix;
		y = Iy;
	}

	GdcPt2(const GdcPt2 &p)
	{
		x = p.x;
		y = p.y;
	}

	bool Inside(class GRect &r);
	
	GdcPt2 operator +(const GdcPt2 &p)
	{
		GdcPt2 r;
		r.x = x + p.x;
		r.y = y + p.y;
		return r;
	}

	GdcPt2 &operator +=(const GdcPt2 &p)
	{
		x += p.x;
		y += p.y;
		return *this;
	}

	GdcPt2 operator -(const GdcPt2 &p)
	{
		GdcPt2 r;
		r.x = x - p.x;
		r.y = y - p.y;
		return r;
	}

	GdcPt2 &operator -=(const GdcPt2 &p)
	{
		x -= p.x;
		y -= p.y;
		return *this;
	}

	GdcPt2 &operator *=(int factor)
	{
		x *= factor;
		y *= factor;
		return *this;
	}

	GdcPt2 &operator /=(int factor)
	{
		x /= factor;
		y /= factor;
		return *this;
	}

	bool operator ==(const GdcPt2 &p)
	{
		return x == p.x && y == p.y;
	}

	bool operator !=(const GdcPt2 &p)
	{
		return !(*this == p);
	}

	void Set(int X, int Y)
	{
		x = X;
		y = Y;
	}

	void Zero()
	{
		x = 0;
		y = 0;
	}
};

/// 3d Point
class LgiClass GdcPt3
{
public:
	int x, y, z;
};

#endif