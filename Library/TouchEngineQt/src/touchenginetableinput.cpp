#include "touchenginetableinput.h"
#include "touchengineinstance.h"
#include <TouchEngine/TouchEngine.h>
#include <QDebug>

TouchEngineTableInput::TouchEngineTableInput(QObject *parent)
    : TouchEngineInputBase(parent)
{
}

TouchEngineTableInput::~TouchEngineTableInput()
{
}

void TouchEngineTableInput::setColumnCount(int count)
{
    if (count < 0) count = 0;
    
    if (m_columnCount != count) {
        m_columnCount = count;
        resizeTableData();
        emit columnCountChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void TouchEngineTableInput::setRowCount(int count)
{
    if (count < 0) count = 0;
    
    if (m_rowCount != count) {
        m_rowCount = count;
        resizeTableData();
        emit rowCountChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void TouchEngineTableInput::setColumnNames(const QStringList &names)
{
    if (m_columnNames != names) {
        m_columnNames = names;
        
        // Adjust column count if necessary
        if (names.size() > m_columnCount) {
            setColumnCount(names.size());
        }
        
        emit columnNamesChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void TouchEngineTableInput::setTableData(const QVariantList &data)
{
    m_tableData = data;
    
    // Infer dimensions if not explicitly set
    if (!data.isEmpty()) {
        m_rowCount = data.size();
        
        if (data[0].canConvert<QVariantList>()) {
            QVariantList firstRow = data[0].toList();
            if (firstRow.size() > m_columnCount) {
                m_columnCount = firstRow.size();
            }
        }
    }
    
    emit tableDataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}

void TouchEngineTableInput::setCellValue(int row, int column, const QVariant &value)
{
    if (row < 0 || row >= m_rowCount || column < 0 || column >= m_columnCount) {
        qWarning() << "Cell index out of bounds:" << row << "," << column;
        return;
    }
    
    // Ensure table data is properly sized
    resizeTableData();
    
    // Access the row
    if (row < m_tableData.size()) {
        QVariantList rowData = m_tableData[row].toList();
        if (column < rowData.size()) {
            rowData[column] = value;
            m_tableData[row] = rowData;
            
            emit cellValueChanged(row, column, value);
            emit tableDataChanged();
            
            if (autoUpdate() && getInstance()) {
                updateValue();
            }
        }
    }
}

QVariant TouchEngineTableInput::getCellValue(int row, int column) const
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

void TouchEngineTableInput::addRow(const QVariantList &rowData)
{
    QVariantList newRow = rowData;
    
    // Ensure row has correct number of columns
    while (newRow.size() < m_columnCount) {
        newRow.append(QVariant());
    }
    
    m_tableData.append(newRow);
    m_rowCount++;
    
    emit rowCountChanged();
    emit tableDataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}

void TouchEngineTableInput::removeRow(int row)
{
    if (row >= 0 && row < m_tableData.size()) {
        m_tableData.removeAt(row);
        m_rowCount = m_tableData.size();
        
        emit rowCountChanged();
        emit tableDataChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void TouchEngineTableInput::addColumn(const QString &columnName)
{
    m_columnCount++;
    
    if (!columnName.isEmpty()) {
        m_columnNames.append(columnName);
    }
    
    // Add empty value to each existing row
    for (int i = 0; i < m_tableData.size(); ++i) {
        QVariantList rowData = m_tableData[i].toList();
        rowData.append(QVariant());
        m_tableData[i] = rowData;
    }
    
    emit columnCountChanged();
    emit columnNamesChanged();
    emit tableDataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}

void TouchEngineTableInput::removeColumn(int column)
{
    if (column >= 0 && column < m_columnCount) {
        m_columnCount--;
        
        if (column < m_columnNames.size()) {
            m_columnNames.removeAt(column);
        }
        
        // Remove value from each row
        for (int i = 0; i < m_tableData.size(); ++i) {
            QVariantList rowData = m_tableData[i].toList();
            if (column < rowData.size()) {
                rowData.removeAt(column);
                m_tableData[i] = rowData;
            }
        }
        
        emit columnCountChanged();
        emit columnNamesChanged();
        emit tableDataChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void TouchEngineTableInput::clear()
{
    m_tableData.clear();
    m_rowCount = 0;
    // Keep column count and names as they define the structure
    
    emit rowCountChanged();
    emit tableDataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}


void TouchEngineTableInput::applyValue(TEInstance* teInstance)
{
    if (!getInstance() || linkName().isEmpty()) {
        return;
    }
    
    // Get the TouchEngine instance handle
    if (!teInstance) {
        emit errorOccurred("TouchEngine instance not initialized");
        return;
    }
    
    QByteArray linkNameUtf8 = linkName().toUtf8();
    
    // Try to get existing table to reuse (more efficient)
    TouchObject<TEObject> existingValue;
    TEResult result = TEInstanceLinkGetObjectValue(teInstance, 
                                                   linkNameUtf8.constData(), 
                                                   TELinkValueCurrent,
                                                   existingValue.take());
    
    TouchObject<TETable> table;
    
    if (result == TEResultSuccess && existingValue && 
        TEGetType(existingValue) == TEObjectTypeTable) {
        // Create a copy of existing table
        table.take(TETableCreateCopy(static_cast<TETable*>(existingValue.get())));
    } else {
        // Create new table
        table.take(TETableCreate());
    }
    
    if (!table) {
        emit errorOccurred("Failed to create TETable");
        return;
    }
    
    // Resize table
    TETableResize(table, m_rowCount, m_columnCount);
    
    int startRow = 0;
    // Set column names if provided
    if (!m_columnNames.isEmpty()) {
        startRow = 1;
        for (int col = 0; col < m_columnCount && col < m_columnNames.size(); ++col) {
            QByteArray colNameUtf8 = m_columnNames[col].toUtf8();
            TETableSetStringValue(table, 0, col, colNameUtf8.constData());
        }
    }
    
    // Set table data
    for (int row = 0; row < m_rowCount && row < m_tableData.size(); ++row) {
        QVariantList rowData;
        
        if (m_tableData[row].canConvert<QVariantList>()) {
            rowData = m_tableData[row].toList();
        }
        
        for (int col = 0; col < m_columnCount && col < rowData.size(); ++col) {
            QString cellValue = variantToString(rowData[col]);
            QByteArray cellUtf8 = cellValue.toUtf8();
            
            // TETable supports different data types per cell
            // For now, we're setting everything as strings
            TETableSetStringValue(table, row+startRow, col, cellUtf8.constData());
        }
    }
    
    // Set the table value on the link
    result = TEInstanceLinkSetTableValue(teInstance, linkNameUtf8.constData(), table);
    
    if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to set table value on link '%1': %2")
                          .arg(linkName())
                          .arg(result);
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
    }
}

void TouchEngineTableInput::resizeTableData()
{
    // Ensure we have the right number of rows
    while (m_tableData.size() < m_rowCount) {
        QVariantList emptyRow;
        for (int i = 0; i < m_columnCount; ++i) {
            emptyRow.append(QVariant());
        }
        m_tableData.append(emptyRow);
    }
    
    // Trim excess rows
    while (m_tableData.size() > m_rowCount) {
        m_tableData.removeLast();
    }
    
    // Ensure each row has the right number of columns
    for (int row = 0; row < m_tableData.size(); ++row) {
        QVariantList rowData = m_tableData[row].toList();
        
        while (rowData.size() < m_columnCount) {
            rowData.append(QVariant());
        }
        
        while (rowData.size() > m_columnCount) {
            rowData.removeLast();
        }
        
        m_tableData[row] = rowData;
    }
}

QString TouchEngineTableInput::variantToString(const QVariant &value) const
{
    if (value.isNull() || !value.isValid()) {
        return QString();
    }
    
    return value.toString();
}
