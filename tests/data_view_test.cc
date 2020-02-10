/*
 * data_view_test.cc
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

#include "gtest/gtest.h"

#include "log.h"
#include "helpers.h"
#include "data_view.h"

#include <cstring>

using namespace std;
using namespace ael;

TEST(DataView, Basic) {
	auto msg1 = "hello world";
	auto msg1_len = strlen(msg1);

	auto view1 = DataView(reinterpret_cast<const uint8_t*>(msg1), msg1_len);

	ASSERT_EQ(msg1_len, view1.GetDataLength());
	ASSERT_EQ(reinterpret_cast<const uint8_t*>(msg1), view1.GetData());
	ASSERT_EQ(0, memcmp(msg1, view1.GetData(), view1.GetDataLength()));

	auto view1_save = view1.Save();

	ASSERT_EQ(view1.GetDataLength(), view1_save->GetDataLength());
	ASSERT_NE(view1.GetData(), view1_save->GetData());
	ASSERT_EQ(0, memcmp(view1.GetData(), view1_save->GetData(), view1.GetDataLength()));

	auto view1_save_save = view1_save->Save();
	ASSERT_EQ(view1_save.get(), view1_save_save.get());
	ASSERT_EQ(0, memcmp(view1.GetData(), view1_save_save->GetData(), view1.GetDataLength()));

	auto copy_ctor_view = view1;

	ASSERT_EQ(view1.GetDataLength(), copy_ctor_view.GetDataLength());
	ASSERT_EQ(view1.GetData(), copy_ctor_view.GetData());
	ASSERT_EQ(0, memcmp(view1.GetData(), copy_ctor_view.GetData(), view1.GetDataLength()));
}

TEST(DataView, Slice) {
	auto msg = "hello";
	auto msg_len = strlen(msg);

	auto view = DataView(reinterpret_cast<const uint8_t*>(msg), msg_len);

	EXPECT_ANY_THROW(view.Slice(-1));
	EXPECT_NO_THROW(view.Slice(5));
	EXPECT_ANY_THROW(view.Slice(6));
	EXPECT_ANY_THROW(view.Slice(1000));
	EXPECT_NO_THROW(view.Slice(0));

	auto sliced_view = view.Slice(1);
	ASSERT_EQ(0, memcmp(sliced_view.GetData(), "ello", sliced_view.GetDataLength()));
	auto sliced_sliced_view = sliced_view.Slice(3);
	ASSERT_EQ(0, memcmp(sliced_sliced_view.GetData(), "o", sliced_sliced_view.GetDataLength()));
	EXPECT_NO_THROW(sliced_sliced_view.Slice(1));

}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    log::Sink::sink_ = new CoutSink();

    return RUN_ALL_TESTS();
}
