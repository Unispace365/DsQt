#include "dsQmlTouchEngineTableOutput.h"
#include "dsQmlTouchEngineInstance.h"
#include <TouchEngine/TouchEngine.h>
#include <TouchEngine/TouchObject.h>
#include <QDebug>

DsQmlTouchEngineTableOutput::DsQmlTouchEngineTableOutput(QObject *parent)
    : DsQmlTouchEngineOutputBase(parent)
{
}

DsQmlTouchEngineTableOutput::~DsQmlTouchEngineTableOutput()
{
}

QVariant DsQmlTouchEngineTableOutput::getCellValue(int row, int column) const
{
    if (row < 0 || row >= m_rowCount || column < 0 || column >= m_columnCount) {
        return QVariant();
    }

    if (row < m_tableData.size()) {
        QVariantList rowData = m_tableData[row].toList();
        if (column < rowData.size()) {
            return rowData[column];
        }
    }

    return QVariant();
}

QString DsQmlTouchEngineTableOutput::getCellString(int row, int column) const
{
    return getCellValue(row, column).toString();
}

void DsQmlTouchEngineTableOutput::readValue(TEInstance* teInstance)
{
    if (!teInstance || linkName().isEmpty()) {
        return;
    }

    QByteArray linkNameUtf8 = linkName().toUtf8();

    // Get the table value from the output link
    TouchObject<TETable> table;
    TEResult result = TEInstanceLinkGetTableValue(teInstance,
                                                   linkNameUtf8.constData(),
                                                   TELinkValueCurrent,
                                                   table.take());

    if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to get table value from link '%1': %2")
                          .arg(linkName())
                          .arg(TEResultGetDescription(result));
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        return;
    }

    if (!table) {
        return;
    }

    // Get table dimensions
    int32_t newRowCount = TETableGetRowCount(table);
    int32_t newColumnCount = TETableGetColumnCount(table);

    // Read column names (first row if it contains headers)
    QStringList newColumnNames;
    for (int32_t col = 0; col < newColumnCount; ++col) {
        const char* str = TETableGetStringValue(table, 0, col);
        newColumnNames.append(QString::fromUtf8(str ? str : ""));
    }

    // Read table data
    QVariantList newTableData;
    for (int32_t row = 0; row < newRowCount; ++row) {
        QVariantList rowData;
        for (int32_t col = 0; col < newColumnCount; ++col) {
            const char* str = TETableGetStringValue(table, row, col);
            rowData.append(QString::fromUtf8(str ? str : ""));
        }
        newTableData.append(QVariant::fromValue(rowData));
    }

    // Update values and emit signals
    bool columnCountChanged = (newColumnCount != m_columnCount);
    bool rowCountChanged = (newRowCount != m_rowCount);
    bool columnNamesChanged = (newColumnNames != m_columnNames);
    bool tableDataChanged = (newTableData != m_tableData);

    if (columnCountChanged) {
        m_columnCount = newColumnCount;
        emit this->columnCountChanged();
    }

    if (rowCountChanged) {
        m_rowCount = newRowCount;
        emit this->rowCountChanged();
    }

    if (columnNamesChanged) {
        m_columnNames = newColumnNames;
        emit this->columnNamesChanged();
    }

    if (tableDataChanged) {
        m_tableData = newTableData;
        emit this->tableDataChanged();
        emit valueUpdated();
    }
}
