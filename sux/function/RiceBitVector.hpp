/*
 * Sux: Succinct data structures
 *
 * Copyright (C) 2019 Emmanuel Esposito and Sebastiano Vigna
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License as published by the Free
 *  Software Foundation; either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  This library is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "../support/common.hpp"
#include "../util/Vector.hpp"
#include <cstdint>
#include <cstdio>
#include <iostream>

namespace sux::function {

using namespace std;
using namespace sux;

/** Storage for Golomb-Rice codes of a RecSplit bucket.
 *
 * This class exists solely to implement RecSplit.
 * @tparam AT a type of memory allocation out of util::AllocType.
 */

template <util::AllocType AT = util::AllocType::MALLOC> class RiceBitVector {

  public:
	class Builder {
		util::Vector<uint64_t, AT> data;
		size_t bit_count = 0;

	  public:
		Builder() : Builder(16) {}

		Builder(const size_t alloc_words) : data(alloc_words) {}

		void appendFixed(const uint64_t v, const int log2golomb) {
			const uint64_t lower_bits = v & ((uint64_t(1) << log2golomb) - 1);
			int used_bits = bit_count & 63;

			data.resize((((bit_count + log2golomb + 7) / 8) + 7 + 7) / 8);

			uint64_t *append_ptr = data.p() + bit_count / 64;
			uint64_t cur_word = *append_ptr;

			cur_word |= lower_bits << used_bits;
			if (used_bits + log2golomb > 64) {
				*(append_ptr++) = cur_word;
				cur_word = lower_bits >> (64 - used_bits);
				used_bits += log2golomb - 64;
			}
			*append_ptr = cur_word;
			bit_count += log2golomb;
		}

		void appendUnaryAll(const std::vector<uint32_t> unary) {
			size_t bit_inc = 0;
			for (const auto &u : unary) {
				bit_inc += u + 1;
			}

			data.resize((((bit_count + bit_inc + 7) / 8) + 7 + 7) / 8);

			for (const auto &u : unary) {
				bit_count += u;
				uint64_t *append_ptr = data.p() + bit_count / 64;
				*append_ptr |= uint64_t(1) << (bit_count & 63);
				++bit_count;
			}
		}

		uint64_t getBits() { return bit_count; }

		RiceBitVector<AT> build() {
			data.trimToFit();
			return RiceBitVector(std::move(data));
		}
	};

  private:
	util::Vector<uint64_t, AT> data;
	size_t curr_fixed_offset = 0;
	uint64_t curr_window_unary = 0;
	uint64_t *curr_ptr_unary;
	int valid_lower_bits_unary = 0;

	friend std::ostream &operator<<(std::ostream &os, const RiceBitVector<AT> &rbv) {
		// os.write((char *)&rbv.bit_count, sizeof(rbv.bit_count));
		os << rbv.data;
		return os;
	}

	friend std::istream &operator>>(std::istream &is, RiceBitVector<AT> &rbv) {
		rbv.curr_fixed_offset = 0;
		rbv.curr_window_unary = 0;
		rbv.valid_lower_bits_unary = 0;
		// is.read((char *)&rbv.bit_count, sizeof(rbv.bit_count));
		is >> rbv.data;
		return is;
	}

  public:
	RiceBitVector() {}
	RiceBitVector(util::Vector<uint64_t, AT> data) : data(std::move(data)) {}

	uint64_t readNext(const int log2golomb) {
		uint64_t result = 0;

		if (curr_window_unary == 0) {
			result += valid_lower_bits_unary;
			curr_window_unary = *(curr_ptr_unary++);
			valid_lower_bits_unary = 64;
			while (__builtin_expect(curr_window_unary == 0, 0)) {
				result += 64;
				curr_window_unary = *(curr_ptr_unary++);
			}
		}

		const size_t pos = rho(curr_window_unary);

		curr_window_unary >>= pos;
		curr_window_unary >>= 1;
		valid_lower_bits_unary -= pos + 1;

		result += pos;
		result <<= log2golomb;

		uint64_t fixed;
		memcpy(&fixed, (uint8_t *)data.p() + curr_fixed_offset / 8, 8);
		result |= (fixed >> curr_fixed_offset % 8) & ((uint64_t(1) << log2golomb) - 1);
		curr_fixed_offset += log2golomb;
		return result;
	}

	void skipSubtree(const size_t nodes, const size_t fixed_len) {
		assert(nodes > 0);
		size_t missing = nodes, cnt;
		while ((cnt = nu(curr_window_unary)) < missing) {
			curr_window_unary = *(curr_ptr_unary++);
			missing -= cnt;
			valid_lower_bits_unary = 64;
		}
		cnt = select64(curr_window_unary, missing - 1);
		curr_window_unary >>= cnt;
		curr_window_unary >>= 1;
		valid_lower_bits_unary -= cnt + 1;

		curr_fixed_offset += fixed_len;
	}

	void readReset(const size_t bit_pos, const size_t unary_offset) {
		// assert(bit_pos < bit_count);
		curr_fixed_offset = bit_pos;
		size_t unary_pos = bit_pos + unary_offset;
		curr_ptr_unary = data.p() + unary_pos / 64;
		curr_window_unary = *(curr_ptr_unary++) >> (unary_pos & 63);
		valid_lower_bits_unary = 64 - (unary_pos & 63);
	}

	size_t getBits() const { return data.size() * sizeof(uint64_t); }
};

} // namespace sux::function
