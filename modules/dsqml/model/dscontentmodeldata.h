#ifndef DSCONTENTMODELDATA_H
#define DSCONTENTMODELDATA_H

#include <QSharedData>


class DSContentModelData : public QSharedData {
  public:
	DSContentModelData();
	DSContentModelData(const DSContentModelData &other);
	~DSContentModelData();

  private:
};

#endif	// DSCONTENTMODELDATA_H
