/*
 * data_view.h
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

#ifndef LIB_DATA_VIEW_H_
#define LIB_DATA_VIEW_H_

#include <cstdint>
#include <memory>

namespace ael {

class DataView : public std::enable_shared_from_this<DataView>  {
public:
	DataView();
	DataView(const DataView& other);
	DataView(const std::uint8_t* data, int data_length);

	virtual ~DataView();

	const std::uint8_t* GetData() const { return data_; }
	int GetDataLength() const { return data_length_; }

	// [suffix_index, data_length_)
	DataView Slice(int suffix_index) const;

	std::shared_ptr<const DataView> Save() const;

private:
	const std::uint8_t* const data_;
	const int data_length_;
	const bool saved_;

	DataView(const std::uint8_t* data, int data_length, bool saved);

};

} /* namespace ael */

#endif /* LIB_DATA_VIEW_H_ */
