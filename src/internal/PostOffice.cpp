#include "PostOffice.h"

#include "internal/Van.h"
#include "internal/Customer.h"

namespace ps {

PostOffice::~PostOffice() {
	delete van_;
}


} // namespace ps


