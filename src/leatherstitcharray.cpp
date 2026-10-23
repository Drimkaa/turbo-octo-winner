#include "leatherstitcharray.h"

#include "document_interface.h"
#include "geometry.h"
#include "stitchdialog.h"

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointF>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kGeometryTolerance = 1.0e-9;
constexpr double kDuplicateTolerance = 0.001;

void deleteEntities(QList<Plug_Entity*>& entities)
{
    while (!entities.isEmpty()) {
        delete entities.takeFirst();
    }
}

double pointDistance(stitch::Point first, stitch::Point last)
{
    return std::hypot(last.x - first.x, last.y - first.y);
}

double positiveSweep(double start, double end)
{
    double sweep = std::fmod(end - start, kTwoPi);
    if (sweep <= 0.0) {
        sweep += kTwoPi;
    }
    return sweep;
}

stitch::Path pathFromEntity(Plug_Entity* entity)
{
    QHash<int, QVariant> data;
    entity->getData(&data);
    const int type = data.value(DPI::ETYPE).toInt();
    stitch::Path path;

    if (type == DPI::LINE) {
        const stitch::Point start{data.value(DPI::STARTX).toDouble(),
                                  data.value(DPI::STARTY).toDouble()};
        const stitch::Point end{data.value(DPI::ENDX).toDouble(),
                                data.value(DPI::ENDY).toDouble()};
        if (pointDistance(start, end) <= kGeometryTolerance) {
            throw std::invalid_argument("The selected line has zero length");
        }
        path.segments.push_back(stitch::Segment::line(start, end));
        return path;
    }

    if (type == DPI::ARC) {
        const stitch::Point center{data.value(DPI::STARTX).toDouble(),
                                   data.value(DPI::STARTY).toDouble()};
        const double radius = data.value(DPI::RADIUS).toDouble();
        const double startAngle = data.value(DPI::STARTANGLE).toDouble();
        const double endAngle = data.value(DPI::ENDANGLE).toDouble();
        const bool reversed = data.value(DPI::REVERSED).toBool();
        const double ccwSweep = positiveSweep(startAngle, endAngle);
        const double sweep = reversed ? -(kTwoPi - ccwSweep) : ccwSweep;
        path.segments.push_back(stitch::Segment::arc(center, radius, startAngle, sweep));
        return path;
    }

    if (type != DPI::POLYLINE) {
        throw std::invalid_argument("Select one LINE, ARC, or POLYLINE entity");
    }

    QList<Plug_VertexData> vertices;
    entity->getPolylineData(&vertices);
    path.closed = data.value(DPI::CLOSEPOLY).toBool();
    if (vertices.size() < 2) {
        throw std::invalid_argument("The selected polyline has fewer than two vertices");
    }

    const int segmentCount = path.closed ? vertices.size() : vertices.size() - 1;
    for (int index = 0; index < segmentCount; ++index) {
        const Plug_VertexData& first = vertices.at(index);
        const Plug_VertexData& last = vertices.at((index + 1) % vertices.size());
        const stitch::Point start{first.point.x(), first.point.y()};
        const stitch::Point end{last.point.x(), last.point.y()};
        if (pointDistance(start, end) <= kGeometryTolerance) {
            continue;
        }
        path.segments.push_back(stitch::segmentFromBulge(start, end, first.bulge));
    }

    if (path.segments.empty()) {
        throw std::invalid_argument("The selected polyline has no usable segments");
    }
    return path;
}

bool sameCircle(const stitch::Point& point, double radius, const QString& layer,
                const QHash<int, QVariant>& data)
{
    return data.value(DPI::ETYPE).toInt() == DPI::CIRCLE &&
           data.value(DPI::LAYER).toString() == layer &&
           std::abs(data.value(DPI::STARTX).toDouble() - point.x) <= kDuplicateTolerance &&
           std::abs(data.value(DPI::STARTY).toDouble() - point.y) <= kDuplicateTolerance &&
           std::abs(data.value(DPI::RADIUS).toDouble() - radius) <= kDuplicateTolerance;
}

std::vector<QHash<int, QVariant>> existingHoleCircles(Document_Interface* document,
                                                      const QString& layer)
{
    QList<Plug_Entity*> entities;
    std::vector<QHash<int, QVariant>> circles;
    if (document->getAllEntities(&entities, false)) {
        circles.reserve(static_cast<std::size_t>(entities.size()));
        for (Plug_Entity* entity : entities) {
            QHash<int, QVariant> data;
            entity->getData(&data);
            if (data.value(DPI::ETYPE).toInt() == DPI::CIRCLE &&
                data.value(DPI::LAYER).toString() == layer) {
                circles.push_back(data);
            }
        }
    }
    deleteEntities(entities);
    return circles;
}

} // namespace

QString LC_LeatherStitchArray::name() const
{
    return tr("Leather Stitch Array");
}

PluginCapabilities LC_LeatherStitchArray::getCapabilities() const
{
    PluginCapabilities capabilities;
    capabilities.menuEntryPoints
        << PluginMenuLocation(QStringLiteral("plugins_menu"), tr("Leather Stitch Array"));
    return capabilities;
}

void LC_LeatherStitchArray::execComm(Document_Interface* document, QWidget* parent, QString command)
{
    Q_UNUSED(command)
    if (!document) {
        return;
    }

    if (toolbar_) {
        delete toolbar_;
    }

    int drawingUnits = 0;
    if (document->getVariableInt(QStringLiteral("$INSUNITS"), &drawingUnits) && drawingUnits != 4) {
        QMessageBox::critical(parent, tr("Leather Stitch Array"),
                              tr("The current drawing is not configured in millimeters."));
        return;
    }

    QList<Plug_Entity*> selected;
    if (!document->getSelect(&selected, tr("Select one LINE, ARC, or POLYLINE, then press Enter"))) {
        deleteEntities(selected);
        return;
    }
    if (selected.size() != 1) {
        QMessageBox::critical(parent, tr("Leather Stitch Array"),
                              tr("Select exactly one LINE, ARC, or POLYLINE entity."));
        deleteEntities(selected);
        return;
    }

    try {
        path_ = pathFromEntity(selected.first());
        deleteEntities(selected);

        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent ? parent->window() : nullptr);
        if (!mainWindow) {
            QMessageBox::critical(parent, tr("Leather Stitch Array"),
                                  tr("LibreCAD's main window is not available."));
            return;
        }
        document_ = document;
        parent_ = parent;
        toolbar_ = new StitchToolbar(path_, mainWindow);
        toolbar_->setAttribute(Qt::WA_DeleteOnClose);
        connect(toolbar_, &StitchToolbar::createRequested,
                this, &LC_LeatherStitchArray::createArray);
        connect(toolbar_, &QObject::destroyed, this, [this]() {
            toolbar_ = nullptr;
            document_ = nullptr;
            parent_ = nullptr;
        });
        mainWindow->addToolBar(Qt::TopToolBarArea, toolbar_);
        toolbar_->show();
    } catch (const std::exception& error) {
        deleteEntities(selected);
        QMessageBox::critical(parent, tr("Leather Stitch Array"),
                              QString::fromLocal8Bit(error.what()));
    }
}

void LC_LeatherStitchArray::createArray()
{
    if (!toolbar_ || !document_) {
        return;
    }

    try {
        stitch::Path path = toolbar_->reverseDirection() ? stitch::reversed(path_) : path_;
        stitch::DistributionOptions options;
        options.spacing = toolbar_->spacing();
        options.startOffset = toolbar_->startOffset();
        options.mode = toolbar_->distributionMode();
        const stitch::DistributionResult result = stitch::distribute(path, options);

        const double radius = toolbar_->diameter() * 0.5;
        const QString targetLayer = document_->getCurrentLayer();
        const std::vector<QHash<int, QVariant>> existing =
            existingHoleCircles(document_, targetLayer);
        std::size_t created = 0;
        std::size_t skipped = 0;
        for (const stitch::Point& point : result.points) {
            const bool duplicate = std::any_of(existing.begin(), existing.end(),
                [&](const QHash<int, QVariant>& data) {
                    return sameCircle(point, radius, targetLayer, data);
                });
            if (duplicate) {
                ++skipped;
                continue;
            }
            QPointF center(point.x, point.y);
            document_->addCircle(&center, radius);
            ++created;
        }
        document_->updateView();

        QString summary = tr("Created %1 hole(s) on the current layer.").arg(created);
        if (skipped > 0) {
            summary += tr("\nSkipped %1 existing duplicate(s).").arg(skipped);
        }
        QWidget* messageParent = parent_;
        toolbar_->close();
        QMessageBox::information(messageParent, tr("Leather Stitch Array"), summary);
    } catch (const std::exception& error) {
        QMessageBox::critical(parent_, tr("Leather Stitch Array"),
                              QString::fromLocal8Bit(error.what()));
    }
}
