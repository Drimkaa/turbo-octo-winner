#ifndef LEATHER_STITCH_GEOMETRY_H
#define LEATHER_STITCH_GEOMETRY_H

#include <cstddef>
#include <string>
#include <vector>

namespace stitch {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct Segment {
    enum class Kind { Line, Arc };

    Kind kind = Kind::Line;
    Point start;
    Point end;
    Point center;
    double radius = 0.0;
    double startAngle = 0.0;
    double sweep = 0.0;

    static Segment line(Point start, Point end);
    static Segment arc(Point center, double radius, double startAngle, double sweep);

    double length() const;
    Point pointAtDistance(double distance) const;
};

struct Path {
    std::vector<Segment> segments;
    bool closed = false;

    double length() const;
    Point pointAtDistance(double distance) const;
};

enum class DistributionMode { ExactPitch, FitToPerimeter };

struct DistributionOptions {
    double spacing = 4.0;
    double startOffset = 0.0;
    DistributionMode mode = DistributionMode::ExactPitch;
    std::size_t maximumPoints = 100000;
};

struct DistributionResult {
    std::vector<Point> points;
    double actualSpacing = 0.0;
    double closingGap = 0.0;
    std::string warning;
};

Segment segmentFromBulge(Point start, Point end, double bulge);
Path reversed(const Path& path);
DistributionResult distribute(const Path& path, const DistributionOptions& options);

} // namespace stitch

#endif
