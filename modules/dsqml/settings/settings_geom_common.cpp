#include "settings.h"
#include <algorithm>
#include <string>




namespace dsqt {

const GeomElements getGeomElementsFromTable(toml::node_view<toml::node> node) {


	toml::table& nTable = *(node.as_table());
	GeomElements out;
	out.x  = nTable["x"].value<double>();
	out.y  = nTable["y"].value<double>();
	out.z  = nTable["z"].value<double>();
	out.w  = nTable["w"].value<double>();
	out.h  = nTable["h"].value<double>();
	out.x1 = nTable["x1"].value<double>();
	out.y1 = nTable["y1"].value<double>();
	out.x2 = nTable["x2"].value<double>();
	out.y2 = nTable["y2"].value<double>();
	return out;
}


}  // namespace dsqt
