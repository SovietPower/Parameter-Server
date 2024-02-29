#include "PostOffice.h"

#include "internal/Van.h"
#include "internal/Customer.h"

namespace ps {

PostOffice::~PostOffice() {
	delete van_;
}

void PostOffice::InitEnv() {

}


void PostOffice::Start(int customer_id, const char* argv0, bool need_barrier) {

}

void PostOffice::Finalize(int customer_id, bool need_barrier) {

}

void PostOffice::AddCustomer(Customer* customer) {

}

void PostOffice::RemoveCustomer(Customer* customer) {

}

Customer* PostOffice::GetCustomer(int app_id, int customer_id, int timeout_in_sec) {

}

void PostOffice::Barrier(int customer_id, int group_id) {

}

void PostOffice::ExitBarrier(const Message& msg) {

}


int PostOffice::my_rank() const  {
	return IDToRank(van_->my_node().id);
}

bool PostOffice::is_recovered() const {
	return van_->my_node().is_recovered;
}

const std::vector<Range>& PostOffice::GetServerRanges() {

}

std::vector<int> PostOffice::GetDeadNodes(int time_in_sec) {

}



} // namespace ps


