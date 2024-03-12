#ifndef CONTENTMODELDATA_H
#define CONTENTMODELDATA_H

#include <QSharedDataPointer>

class ContentModelDataData;

class ContentModelData : public QSharedData {
  public:
	ContentModelData();
	ContentModelData(const ContentModelData &);
	ContentModelData &operator=(const ContentModelData &);
	~ContentModelData();

  private:
	QSharedDataPointer<ContentModelDataData> data;
};

#endif	// CONTENTMODELDATA_H
