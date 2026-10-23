#ifndef LEATHER_STITCH_ARRAY_H
#define LEATHER_STITCH_ARRAY_H

#include "geometry.h"
#include "qc_plugininterface.h"

#include <QObject>
#include <QPointer>

class StitchToolbar;

class LC_LeatherStitchArray : public QObject, public QC_PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(QC_PluginInterface)
    Q_PLUGIN_METADATA(IID LC_DocumentInterface_iid FILE "leatherstitcharray.json")

public:
    QString name() const override;
    PluginCapabilities getCapabilities() const override;
    void execComm(Document_Interface* document, QWidget* parent, QString command) override;

private slots:
    void createArray();

private:
    QPointer<StitchToolbar> toolbar_;
    Document_Interface* document_ = nullptr;
    QWidget* parent_ = nullptr;
    stitch::Path path_;
};

#endif
