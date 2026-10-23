#include "geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace stitch {
namespace {

constexpr double kEpsilon = 1.0e-10;
constexpr double kTwoPi = 6.283185307179586476925286766559;

double distance(Point a, Point b)
{
    return std::hypot(b.x - a.x, b.y - a.y);
}

Point pointOnArc(const Segment& segment, double fraction)
{
    const double angle = segment.startAngle + segment.sweep * fraction;
    return {segment.center.x + segment.radius * std::cos(angle),
            segment.center.y + segment.radius * std::sin(angle)};
}

} // namespace

Segment Segment::line(Point first, Point last)
{
    Segment segment;
    segment.start = first;
    segment.end = last;
    return segment;
}

Segment Segment::arc(Point arcCenter, double arcRadius, double angle, double arcSweep)
{
    if (!std::isfinite(arcRadius) || arcRadius <= kEpsilon ||
        !std::isfinite(angle) || !std::isfinite(arcSweep) || std::abs(arcSweep) <= kEpsilon) {
        throw std::invalid_argument("Arc radius and sweep must be non-zero finite values");
    }

    Segment segment;
    segment.kind = Kind::Arc;
    segment.center = arcCenter;
    segment.radius = arcRadius;
    segment.startAngle = angle;
    segment.sweep = arcSweep;
    segment.start = pointOnArc(segment, 0.0);
    segment.end = pointOnArc(segment, 1.0);
    return segment;
}

double Segment::length() const
{
    return kind == Kind::Line ? distance(start, end) : radius * std::abs(sweep);
}

Point Segment::pointAtDistance(double along) const
{
    const double segmentLength = length();
    if (segmentLength <= kEpsilon) {
        return start;
    }
    const double fraction = std::clamp(along / segmentLength, 0.0, 1.0);
    if (kind == Kind::Arc) {
        return pointOnArc(*this, fraction);
    }
    return {start.x + (end.x - start.x) * fraction,
            start.y + (end.y - start.y) * fraction};
}

double Path::length() const
{
    double total = 0.0;
    for (const Segment& segment : segments) {
        total += segment.length();
    }
    return total;
}

Point Path::pointAtDistance(double along) const
{
    if (segments.empty()) {
        throw std::invalid_argument("Path has no usable segments");
    }

    const double total = length();
    double remaining = std::clamp(along, 0.0, total);
    for (const Segment& segment : segments) {
        const double segmentLength = segment.length();
        if (remaining <= segmentLength + kEpsilon) {
            return segment.pointAtDistance(remaining);
        }
        remaining -= segmentLength;
    }
    return segments.back().end;
}

Segment segmentFromBulge(Point first, Point last, double bulge)
{
    const double chord = distance(first, last);
    if (!std::isfinite(bulge) || chord <= kEpsilon) {
        throw std::invalid_argument("Bulge segment requires distinct finite points");
    }
    if (std::abs(bulge) <= kEpsilon) {
        return Segment::line(first, last);
    }

    const double dx = last.x - first.x;
    const double dy = last.y - first.y;
    const double centerOffset = chord * (1.0 - bulge * bulge) / (4.0 * bulge);
    const Point midpoint{(first.x + last.x) * 0.5, (first.y + last.y) * 0.5};
    const Point center{midpoint.x - dy * centerOffset / chord,
                       midpoint.y + dx * centerOffset / chord};
    const double radius = chord * (1.0 + bulge * bulge) / (4.0 * std::abs(bulge));
    const double startAngle = std::atan2(first.y - center.y, first.x - center.x);
    return Segment::arc(center, radius, startAngle, 4.0 * std::atan(bulge));
}

Path reversed(const Path& path)
{
    Path result;
    result.closed = path.closed;
    result.segments.reserve(path.segments.size());
    for (auto it = path.segments.rbegin(); it != path.segments.rend(); ++it) {
        if (it->kind == Segment::Kind::Line) {
            result.segments.push_back(Segment::line(it->end, it->start));
        } else {
            result.segments.push_back(Segment::arc(
                it->center, it->radius, it->startAngle + it->sweep, -it->sweep));
        }
    }
    return result;
}

DistributionResult distribute(const Path& path, const DistributionOptions& options)
{
    const double total = path.length();
    if (!std::isfinite(options.spacing) || options.spacing <= kEpsilon) {
        throw std::invalid_argument("Spacing must be greater than zero");
    }
    if (!std::isfinite(options.startOffset) || options.startOffset < 0.0) {
        throw std::invalid_argument("Start offset must be a non-negative finite value");
    }
    if (total <= kEpsilon || path.segments.empty()) {
        throw std::invalid_argument("Path length must be greater than zero");
    }

    DistributionResult result;
    double firstDistance = options.startOffset;
    std::size_t count = 0;

    if (path.closed) {
        firstDistance = std::fmod(firstDistance, total);
        if (options.mode == DistributionMode::FitToPerimeter) {
            const std::size_t intervals = std::max<std::size_t>(
                1, static_cast<std::size_t>(std::llround(total / options.spacing)));
            count = intervals;
            result.actualSpacing = total / static_cast<double>(intervals);
            result.closingGap = result.actualSpacing;
            if (std::abs(result.actualSpacing - options.spacing) > 0.005) {
                result.warning = "Actual spacing differs from the requested punch spacing";
            }
        } else {
            count = std::max<std::size_t>(
                1, static_cast<std::size_t>(std::floor(total / options.spacing + kEpsilon)));
            result.actualSpacing = options.spacing;
            result.closingGap = total - static_cast<double>(count - 1) * options.spacing;
        }
    } else {
        if (firstDistance > total + kEpsilon) {
            throw std::invalid_argument("Start offset exceeds the open path length");
        }
        count = static_cast<std::size_t>(
            std::floor((total - std::min(firstDistance, total)) / options.spacing + kEpsilon)) + 1;
        result.actualSpacing = options.spacing;
    }

    if (count > options.maximumPoints) {
        throw std::length_error("Requested array exceeds the safety limit");
    }

    result.points.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        double along = firstDistance + static_cast<double>(index) * result.actualSpacing;
        if (path.closed) {
            along = std::fmod(along, total);
        }
        result.points.push_back(path.pointAtDistance(along));
    }
    return result;
}

} // namespace stitch
