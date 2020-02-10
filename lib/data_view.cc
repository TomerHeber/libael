/*
 * data_view.cc
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

#include "data_view.h"

#include <cstring>

namespace ael {

DataView::DataView() : data_(nullptr), data_length_(0), saved_(false) {}

DataView::DataView(const DataView& other) : std::enable_shared_from_this<DataView>(), data_(other.data_), data_length_(other.data_length_), saved_(false) {}

DataView::~DataView() {
	if (saved_ && data_ != nullptr) {
		delete [] data_;
	}
}

DataView::DataView(const std::uint8_t* data, int data_length) : data_(data), data_length_(data_length), saved_(false) {}

DataView::DataView(const std::uint8_t* data, int data_length, bool saved) : data_(data), data_length_(data_length), saved_(saved) {}

DataView DataView::Slice(int suffix_index) const {
	if (suffix_index < 0 || suffix_index > data_length_) {
		throw std::out_of_range ("the suffix index is larger than data length");
	}

	if (suffix_index == data_length_) {
		return DataView();
	}

	return DataView(data_ + suffix_index, data_length_ - suffix_index);
}

std::shared_ptr<const DataView> DataView::Save() const {
	if (saved_) {
		return shared_from_this();
	}

	auto data = new std::uint8_t[data_length_];
	std::memcpy(data, data_, data_length_);

	return std::shared_ptr<DataView>(new DataView(data, data_length_, true));
}

} /* namespace ael */
