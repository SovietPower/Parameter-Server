/**
 * @file Van.h
 */
#include "../../internal/Node.h"

namespace ps {

class Van {
 public:

	const Node& my_node() const {
		return my_node_;
	}

 private:
	Node my_node_;

};


} // namespace ps