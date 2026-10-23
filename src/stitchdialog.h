#ifndef LEATHER_STITCH_TOOLBAR_H
#define LEATHER_STITCH_TOOLBAR_H

#include "geometry.h"

#include <QToolBar>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

class StitchToolbar : public QToolBar
{
    Q_OBJECT

public:
    explicit StitchToolbar(const stitch::Path& path, QWidget* parent = nullptr);
    ~StitchToolbar() override;

    double spacing() const;
    double diameter() const;
    double startOffset() const;
    stitch::DistributionMode distributionMode() const;
    bool reverseDirection() const;

signals:
    void createRequested();

private slots:
    void updateMetrics();

private:
    void readSettings();
    void writeSettings() const;

    stitch::Path path_;
    QDoubleSpinBox* spacing_;
    QDoubleSpinBox* diameter_;
    QDoubleSpinBox* offset_;
    QComboBox* mode_;
    QComboBox* direction_;
    QLabel* lengthValue_;
    QLabel* countValue_;
    QLabel* actualSpacingValue_;
    QLabel* closingGapValue_;
    QLabel* warningValue_;
};

#endif
