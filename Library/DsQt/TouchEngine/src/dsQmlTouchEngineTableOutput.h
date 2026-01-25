#ifndef DSQMLTOUCHENGINETABLEOUTPUT_H
#define DSQMLTOUCHENGINETABLEOUTPUT_H

#include "dsQmlTouchEngineOutputBase.h"
#include <QVariantList>
#include <QStringList>

/**
 * DsQmlTouchEngineTableOutput - QML component for reading table/tabular data from TouchEngine output links
 *
 * Example usage in QML:
 * DsTouchEngineTableOutput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "op/data_table_out"
 *     onTableDataChanged: {
 *         console.log("Rows:", rowCount, "Columns:", columnCount)
 *         for (var row = 0; row < rowCount; row++) {
 *             console.log("Row", row, ":", tableData[row])
 *         }
 *     }
 * }
 */
class DsQmlTouchEngineTableOutput : public DsQmlTouchEngineOutputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineTableOutput)

    Q_PROPERTY(int columnCount READ columnCount NOTIFY columnCountChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QStringList columnNames READ columnNames NOTIFY columnNamesChanged)
    Q_PROPERTY(QVariantList tableData READ tableData NOTIFY tableDataChanged)

public:
    explicit DsQmlTouchEngineTableOutput(QObject *parent = nullptr);
    ~DsQmlTouchEngineTableOutput() override;

    int columnCount() const { return m_columnCount; }
    int rowCount() const { return m_rowCount; }
    QStringList columnNames() const { return m_columnNames; }
    QVariantList tableData() const { return m_tableData; }

    // Convenience method for getting cell values
    Q_INVOKABLE QVariant getCellValue(int row, int column) const;
    Q_INVOKABLE QString getCellString(int row, int column) const;

signals:
    void columnCountChanged();
    void rowCountChanged();
    void columnNamesChanged();
    void tableDataChanged();

protected:
    void readValue(TEInstance* teInstance) override;

private:
    int m_columnCount = 0;
    int m_rowCount = 0;
    QStringList m_columnNames;
    QVariantList m_tableData;
};

#endif // DSQMLTOUCHENGINETABLEOUTPUT_H
