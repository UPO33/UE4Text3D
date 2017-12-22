/*
 * FTGL - OpenGL font library
 *
 * Copyright (c) 2001-2004 Henry Maddocks <ftgl@opengl.geek.nz>
 * Copyright (c) 2008 Éric Beets <ericbeets@free.fr>
 * Copyright (c) 2008 Sam Hocevar <sam@zoy.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _DIFONT_CONTOUR_H_
#define _DIFONT_CONTOUR_H_


#include "Engine/FontFace.h"
#include "Engine/Font.h"
#include "Internationalization/Text.h"

#if WITH_FREETYPE
THIRD_PARTY_INCLUDES_START
#include "ft2build.h"

// FreeType style include
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MODULE_H
#include FT_BITMAP_H
#include FT_ADVANCES_H
#include FT_STROKER_H

THIRD_PARTY_INCLUDES_END
#endif // 

#include <vector>



 /**
 * Point class is a basic 3-dimensional point or vector.
 */
class Point {
public:
	/**
	* Default constructor. Point is set to zero.
	*/
	Point()
	{
		values[0] = 0;
		values[1] = 0;
		values[2] = 0;
	}

	/**
	* Constructor. Z coordinate is set to zero if unspecified.
	*
	* @param x First component
	* @param y Second component
	* @param z Third component
	*/
	Point(const double x, const double y,
		const double z = 0)
	{
		values[0] = x;
		values[1] = y;
		values[2] = z;
	}

	/**
	* Constructor. This converts an FT_Vector to an difont::Point
	*
	* @param ft_vector A freetype vector
	*/
	inline Point(const FT_Vector& ft_vector)
	{
		values[0] = ft_vector.x;
		values[1] = ft_vector.y;
		values[2] = 0;
	}

	/**
	* Normalise a point's coordinates. If the coordinates are zero,
	* the point is left untouched.
	*
	* @return A vector of norm one.
	*/
	Point Normalise()
	{
		double norm = sqrt(values[0] * values[0]
			+ values[1] * values[1]
			+ values[2] * values[2]);
		if (norm == 0.0)
		{
			return *this;
		}

		Point temp(values[0] / norm, values[1] / norm, values[2] / norm);
		return temp;
	}


	/**
	* Operator += In Place Addition.
	*
	* @param point
	* @return this plus point.
	*/
	Point& operator += (const Point& point)
	{
		values[0] += point.values[0];
		values[1] += point.values[1];
		values[2] += point.values[2];

		return *this;
	}

	/**
	* Operator +
	*
	* @param point
	* @return this plus point.
	*/
	Point operator + (const Point& point) const
	{
		Point temp;
		temp.values[0] = values[0] + point.values[0];
		temp.values[1] = values[1] + point.values[1];
		temp.values[2] = values[2] + point.values[2];

		return temp;
	}

	/**
	* Operator -= In Place Substraction.
	*
	* @param point
	* @return this minus point.
	*/
	Point& operator -= (const Point& point)
	{
		values[0] -= point.values[0];
		values[1] -= point.values[1];
		values[2] -= point.values[2];

		return *this;
	}

	/**
	* Operator -
	*
	* @param point
	* @return this minus point.
	*/
	Point operator - (const Point& point) const
	{
		Point temp;
		temp.values[0] = values[0] - point.values[0];
		temp.values[1] = values[1] - point.values[1];
		temp.values[2] = values[2] - point.values[2];

		return temp;
	}

	/**
	* Operator *  Scalar multiplication
	*
	* @param multiplier
	* @return <code>this</code> multiplied by <code>multiplier</code>.
	*/
	Point operator * (double multiplier) const
	{
		Point temp;
		temp.values[0] = values[0] * multiplier;
		temp.values[1] = values[1] * multiplier;
		temp.values[2] = values[2] * multiplier;

		return temp;
	}


	/**
	* Operator *  Scalar multiplication
	*
	* @param point
	* @param multiplier
	* @return <code>multiplier</code> multiplied by <code>point</code>.
	*/
	friend Point operator * (double multiplier, Point& point)
	{
		return point * multiplier;
	}


	/**
	* Operator *  Scalar product
	*
	* @param a  First vector.
	* @param b  Second vector.
	* @return  <code>a.b</code> scalar product.
	*/
	friend double operator * (Point &a, Point& b)
	{
		return a.values[0] * b.values[0]
			+ a.values[1] * b.values[1]
			+ a.values[2] * b.values[2];
	}


	/**
	* Operator ^  Vector product
	*
	* @param point Second point
	* @return this vector point.
	*/
	Point operator ^ (const Point& point)
	{
		Point temp;
		temp.values[0] = values[1] * point.values[2]
			- values[2] * point.values[1];
		temp.values[1] = values[2] * point.values[0]
			- values[0] * point.values[2];
		temp.values[2] = values[0] * point.values[1]
			- values[1] * point.values[0];
		return temp;
	}


	/**
	* Operator == Tests for equality
	*
	* @param a
	* @param b
	* @return true if a & b are equal
	*/
	friend bool operator == (const Point &a, const Point &b)
	{
		return((a.values[0] == b.values[0]) && (a.values[1] == b.values[1]) && (a.values[2] == b.values[2]));
	}


	/**
	* Operator != Tests for non equality
	*
	* @param a
	* @param b
	* @return true if a & b are not equal
	*/
	friend bool operator != (const Point &a, const Point &b)
	{
		return((a.values[0] != b.values[0]) || (a.values[1] != b.values[1]) || (a.values[2] != b.values[2]));
	}


	/**
	* Cast to double*
	*/
	operator const double*() const
	{
		return values;
	}


	/**
	* Setters
	*/
	void X(double x) { values[0] = x; };
	void Y(double y) { values[1] = y; };
	void Z(double z) { values[2] = z; };


	/**
	* Getters
	*/
	double X() const { return values[0]; };
	double Y() const { return values[1]; };
	double Z() const { return values[2]; };
	float Xf() const { return static_cast<float>(values[0]); };
	float Yf() const { return static_cast<float>(values[1]); };
	float Zf() const { return static_cast<float>(values[2]); };

private:
	/**
	* The point data
	*/
	double values[3];
};

/**
 * Contour class is a container of points that describe a vector font
 * outline. It is used as a container for the output of the bezier curve
 * evaluator in Vectoriser.
 *
 * @see OutlineGlyph
 * @see PolygonGlyph
 * @see Point
 */

 class  Contour {
    public:
        /**
         * Constructor
         *
         * @param contour
         * @param pointTags
         * @param numberOfPoints
         */
        Contour(FT_Vector* contour, char* pointTags, unsigned int numberOfPoints, unsigned short bezierSteps);

        /**
         * Destructor
         */
        ~Contour()
        {
            pointList.clear();
            outsetPointList.clear();
            frontPointList.clear();
            backPointList.clear();
        }

        /**
         * Return a point at index.
         *
         * @param index of the point in the curve.
         * @return const point reference
         */
        const Point& GetPoint(size_t index) const { return pointList[index]; }

        /**
         * How many points define this contour
         *
         * @return the number of points in this contour
         */
        size_t PointCount() const { return pointList.size(); }

        /**
         * Make sure the glyph has the proper parity and create the front/back
         * outset contour.
         *
         * @param parity  The contour's parity within the glyph.
         */
        void SetParity(int parity);

        /**
         * Make sure the glyph has the proper parity and create the front/back
         * outset contour.
         *
         * @param parity  The contour's parity within the glyph.
         */
        bool GetDirection() const {
            return clockwise;
        }

        bool IsInside(const Contour* big) const {
            if(minx > big->minx && miny > big->miny 
                && maxx < big->maxx && maxy < big->maxy)
                return true;
            else 
                return false;
        }

        float minx, miny, maxx, maxy;
    private:
        /**
         * Add a point to this contour. This function tests for duplicate
         * points.
         *
         * @param point The point to be added to the contour.
         */
        void AddPoint(Point point);

        /**
         * Add a point to this contour. This function tests for duplicate
         * points.
         *
         * @param point The point to be added to the contour.
         */
        void AddOutsetPoint(Point point);


        /**
         * De Casteljau (bezier) algorithm contributed by Jed Soane
         * Evaluates a quadratic or conic (second degree) curve
         */
        void evaluateQuadraticCurve(Point, Point, Point, unsigned short);

        /**
         * De Casteljau (bezier) algorithm contributed by Jed Soane
         * Evaluates a cubic (third degree) curve
         */
        void evaluateCubicCurve(Point, Point, Point, Point, unsigned short);

        /**
         * Compute the outset point coordinates
         */
        Point ComputeOutsetPoint(Point a, Point b, Point c);

        /**
         *  The list of points in this contour
         */
        typedef std::vector<Point> PointVector;
        PointVector pointList;
        PointVector outsetPointList;
        PointVector frontPointList;
        PointVector backPointList;
        
        /**
         *  Is this contour clockwise or anti-clockwise?
         */
        bool clockwise;

};

#endif // __Contour__

