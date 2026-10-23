#include "geometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr double kPi = 3.1415926535897932384626433832795;

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void near(double actual, double expected, const std::string& message, double tolerance = 1.0e-9)
{
    require(std::abs(actual - expected) <= tolerance,
            message + " (expected " + std::to_string(expected) + ", got " +
                std::to_string(actual) + ")");
}

stitch::Path linePath(double length)
{
    return {{stitch::Segment::line({0.0, 0.0}, {length, 0.0})}, false};
}

stitch::Path rectangle(double width, double height)
{
    stitch::Path path;
    path.closed = true;
    path.segments = {
        stitch::Segment::line({0.0, 0.0}, {width, 0.0}),
        stitch::Segment::line({width, 0.0}, {width, height}),
        stitch::Segment::line({width, height}, {0.0, height}),
        stitch::Segment::line({0.0, height}, {0.0, 0.0})};
    return path;
}

} // namespace

int main()
{
    stitch::DistributionOptions exact;

    const auto line100 = stitch::distribute(linePath(100.0), exact);
    require(line100.points.size() == 26, "100 mm line has both endpoints and 26 holes");
    near(line100.points.back().x, 100.0, "100 mm line ends at 100");

    const auto line137 = stitch::distribute(linePath(137.0), exact);
    require(line137.points.size() == 35, "137 mm line has 35 holes");
    near(line137.points.back().x, 136.0, "137 mm line leaves a 1 mm tail");

    const auto divisible = stitch::distribute(rectangle(12.0, 8.0), exact);
    require(divisible.points.size() == 10, "40 mm perimeter has 10 holes");
    near(divisible.closingGap, 4.0, "divisible perimeter closes at pitch");
    require(!(divisible.points.front().x == divisible.points.back().x &&
              divisible.points.front().y == divisible.points.back().y),
            "closed path does not duplicate seam point");

    const auto remainder = stitch::distribute(rectangle(12.5, 8.0), exact);
    require(remainder.points.size() == 10, "41 mm perimeter has 10 exact-pitch holes");
    near(remainder.closingGap, 5.0, "remainder is assigned to closing gap");

    stitch::DistributionOptions fit;
    fit.mode = stitch::DistributionMode::FitToPerimeter;
    const auto fitted = stitch::distribute(rectangle(12.5, 8.0), fit);
    require(fitted.points.size() == 10, "fit uses nearest interval count");
    near(fitted.actualSpacing, 4.1, "fit reports actual spacing");
    require(!fitted.warning.empty(), "fit warns about punch-spacing deviation");

    stitch::Path inclined{{stitch::Segment::line({0.0, 0.0}, {3.0, 4.0})}, false};
    const auto inclinedPoints = stitch::distribute(inclined, exact);
    near(inclinedPoints.points[1].x, 2.4, "inclined X is sampled by path length");
    near(inclinedPoints.points[1].y, 3.2, "inclined Y is sampled by path length");

    const stitch::Segment positive = stitch::segmentFromBulge({0.0, 0.0}, {10.0, 0.0}, 1.0);
    near(positive.length(), 5.0 * kPi, "positive semicircle length");
    const stitch::Point positiveMid = positive.pointAtDistance(positive.length() * 0.5);
    near(positiveMid.x, 5.0, "positive bulge midpoint X");
    near(positiveMid.y, -5.0, "positive bulge follows LibreCAD bulge orientation");

    const stitch::Segment negative = stitch::segmentFromBulge({0.0, 0.0}, {10.0, 0.0}, -1.0);
    const stitch::Point negativeMid = negative.pointAtDistance(negative.length() * 0.5);
    near(negativeMid.y, 5.0, "negative bulge follows the opposite side");

    stitch::Path arcPath{{positive}, false};
    const stitch::Path backwards = stitch::reversed(arcPath);
    near(backwards.pointAtDistance(0.0).x, 10.0, "reversed arc starts at old end");
    near(backwards.pointAtDistance(backwards.length()).x, 0.0, "reversed arc ends at old start");

    bool rejected = false;
    try {
        stitch::segmentFromBulge({1.0, 1.0}, {1.0, 1.0}, 0.5);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "zero-chord bulge is rejected");

    rejected = false;
    try {
        stitch::DistributionOptions unsafe;
        unsafe.spacing = 0.000001;
        unsafe.maximumPoints = 1000;
        stitch::distribute(linePath(100.0), unsafe);
    } catch (const std::length_error&) {
        rejected = true;
    }
    require(rejected, "unbounded hole count is rejected");

    std::cout << "All geometry tests passed\n";
    return EXIT_SUCCESS;
}
