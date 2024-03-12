#include "content_model_data.h"

class ContentModelDataData : public QSharedData {
  public:
};

ContentModelData::ContentModelData() : data(new ContentModelDataData) {}

ContentModelData::ContentModelData(const ContentModelData &rhs) : data{rhs.data} {}

ContentModelData &ContentModelData::operator=(const ContentModelData &rhs) {
	if (this != &rhs) data.operator=(rhs.data);
	return *this;
}

ContentModelData::~ContentModelData() {}
