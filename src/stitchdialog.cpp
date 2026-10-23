#include "stitchdialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSettings>

#include <exception>

namespace {

QString millimeters(double value)
{
    return QLocale().toString(value, 'f', 3) + QStringLiteral(" mm");
}

} // namespace

StitchToolbar::StitchToolbar(const stitch::Path& path, QWidget* parent)
    : QToolBar(tr("Leather Stitch Array"), parent), path_(path)
{
    setObjectName(QStringLiteral("LeatherStitchArrayToolbar"));
    setMovable(true);

    spacing_ = new QDoubleSpinBox(this);
    spacing_->setRange(0.001, 1000000.0);
    spacing_->setDecimals(3);
    spacing_->setSuffix(tr(" mm"));

    diameter_ = new QDoubleSpinBox(this);
    diameter_->setRange(0.001, 1000000.0);
    diameter_->setDecimals(3);
    diameter_->setSuffix(tr(" mm"));

    offset_ = new QDoubleSpinBox(this);
    offset_->setRange(0.0, 1000000000.0);
    offset_->setDecimals(3);
    offset_->setSuffix(tr(" mm"));

    mode_ = new QComboBox(this);
    mode_->addItem(tr("Exact Pitch"));
    mode_->addItem(tr("Fit to Perimeter"));
    mode_->setEnabled(path_.closed);

    direction_ = new QComboBox(this);
    direction_->addItem(tr("Forward"));
    direction_->addItem(tr("Reverse"));

    lengthValue_ = new QLabel(millimeters(path_.length()), this);
    countValue_ = new QLabel(this);
    actualSpacingValue_ = new QLabel(this);
    closingGapValue_ = new QLabel(this);
    warningValue_ = new QLabel(this);
    warningValue_->setStyleSheet(QStringLiteral("color: #a04400"));

    addWidget(new QLabel(tr("Spacing:"), this));
    addWidget(spacing_);
    addSeparator();
    addWidget(new QLabel(tr("Diameter:"), this));
    addWidget(diameter_);
    addSeparator();
    addWidget(new QLabel(tr("Offset:"), this));
    addWidget(offset_);
    addSeparator();
    addWidget(mode_);
    addWidget(direction_);
    addSeparator();
    addWidget(new QLabel(tr("Length:"), this));
    addWidget(lengthValue_);
    addWidget(new QLabel(tr("Holes:"), this));
    addWidget(countValue_);
    addWidget(new QLabel(tr("Actual:"), this));
    addWidget(actualSpacingValue_);
    addWidget(new QLabel(tr("Closing:"), this));
    addWidget(closingGapValue_);
    addWidget(warningValue_);
    addSeparator();

    auto* createButton = new QPushButton(tr("Create"), this);
    auto* cancelButton = new QPushButton(tr("Cancel"), this);
    addWidget(createButton);
    addWidget(cancelButton);

    readSettings();
    connect(spacing_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StitchToolbar::updateMetrics);
    connect(offset_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StitchToolbar::updateMetrics);
    connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StitchToolbar::updateMetrics);
    connect(createButton, &QPushButton::clicked, this, &StitchToolbar::createRequested);
    connect(cancelButton, &QPushButton::clicked, this, &QToolBar::close);
    updateMetrics();
}

StitchToolbar::~StitchToolbar()
{
    writeSettings();
}

double StitchToolbar::spacing() const { return spacing_->value(); }
double StitchToolbar::diameter() const { return diameter_->value(); }
double StitchToolbar::startOffset() const { return offset_->value(); }
bool StitchToolbar::reverseDirection() const { return direction_->currentIndex() == 1; }

stitch::DistributionMode StitchToolbar::distributionMode() const
{
    return mode_->currentIndex() == 1 ? stitch::DistributionMode::FitToPerimeter
                                      : stitch::DistributionMode::ExactPitch;
}

void StitchToolbar::updateMetrics()
{
    try {
        stitch::DistributionOptions options;
        options.spacing = spacing();
        options.startOffset = startOffset();
        options.mode = distributionMode();
        const stitch::DistributionResult result = stitch::distribute(path_, options);
        countValue_->setText(QString::number(result.points.size()));
        actualSpacingValue_->setText(millimeters(result.actualSpacing));
        closingGapValue_->setText(path_.closed ? millimeters(result.closingGap) : tr("Not applicable"));
        warningValue_->setText(QString::fromStdString(result.warning));
    } catch (const std::exception& error) {
        countValue_->setText(QStringLiteral("0"));
        actualSpacingValue_->clear();
        closingGapValue_->clear();
        warningValue_->setText(QString::fromLocal8Bit(error.what()));
    }
}

void StitchToolbar::readSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("LibreCAD"), QStringLiteral("LeatherStitchArray"));
    spacing_->setValue(settings.value(QStringLiteral("spacing"), 4.0).toDouble());
    diameter_->setValue(settings.value(QStringLiteral("diameter"), 1.0).toDouble());
    offset_->setValue(settings.value(QStringLiteral("startOffset"), 0.0).toDouble());
    mode_->setCurrentIndex(path_.closed ? settings.value(QStringLiteral("mode"), 0).toInt() : 0);
    direction_->setCurrentIndex(settings.value(QStringLiteral("direction"), 0).toInt());
}

void StitchToolbar::writeSettings() const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("LibreCAD"), QStringLiteral("LeatherStitchArray"));
    settings.setValue(QStringLiteral("spacing"), spacing());
    settings.setValue(QStringLiteral("diameter"), diameter());
    settings.setValue(QStringLiteral("startOffset"), startOffset());
    settings.setValue(QStringLiteral("mode"), mode_->currentIndex());
    settings.setValue(QStringLiteral("direction"), direction_->currentIndex());
}
