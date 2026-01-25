#ifndef DSQMLTOUCHENGINETABLEINPUT_H
#define DSQMLTOUCHENGINETABLEINPUT_H

#include "dsQmlTouchEngineInputBase.h"
#include <QVariantList>
#include <QStringList>

/**
 * DsQmlTouchEngineTableInput - QML component for setting table/tabular data on TouchEngine links
 *
 * Example usage in QML:
 * DsTouchEngineTableInput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "data_table"
 *     columnCount: 3
 *     rowCount: 2
 *     columnNames: ["Name", "Value", "Type"]
 *
 *     // Set data using 2D array
 *     tableData: [
 *         ["Item1", "100", "Integer"],
 *         ["Item2", "200", "Integer"]
 *     ]
 * }
 */
class DsQmlTouchEngineTableInput : public DsQmlTouchEngineInputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineTableInput)

    Q_PROPERTY(int columnCount READ columnCount WRITE setColumnCount NOTIFY columnCountChanged)
    Q_PROPERTY(int rowCount READ rowCount WRITE setRowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QStringList columnNames READ columnNames WRITE setColumnNames NOTIFY columnNamesChanged)
    Q_PROPERTY(QVariantList tableData READ tableData WRITE setTableData NOTIFY tableDataChanged)

public:
    explicit DsQmlTouchEngineTableInput(QObject *parent = nullptr);
    ~DsQmlTouchEngineTableInput() override;

    int columnCount() const { return m_columnCount; }
    void setColumnCount(int count);

    int rowCount() const { return m_rowCount; }
    void setRowCount(int count);

    QStringList columnNames() const { return m_columnNames; }
    void setColumnNames(const QStringList &names);

    QVariantList tableData() const { return m_tableData; }
    void setTableData(const QVariantList &data);

    // Convenience methods for setting individual cells
    Q_INVOKABLE void setCellValue(int row, int column, const QVariant &value);
    Q_INVOKABLE QVariant getCellValue(int row, int column) const;

    // Methods for adding/removing rows and columns
    Q_INVOKABLE void addRow(const QVariantList &rowData = QVariantList());
    Q_INVOKABLE void removeRow(int row);
    Q_INVOKABLE void addColumn(const QString &columnName = QString());
    Q_INVOKABLE void removeColumn(int column);

    // Clear all data
    Q_INVOKABLE void clear();

signals:
    void columnCountChanged();
    void rowCountChanged();
    void columnNamesChanged();
    void tableDataChanged();
    void cellValueChanged(int row, int column, const QVariant &value);

protected:
    void applyValue(TEInstance* teInstance) override;

private:
    int m_columnCount = 0;
    int m_rowCount = 0;
    QStringList m_columnNames;
    QVariantList m_tableData;

    void resizeTableData();
    QString variantToString(const QVariant &value) const;
};

#endif // DSQMLTOUCHENGINETABLEINPUT_H
