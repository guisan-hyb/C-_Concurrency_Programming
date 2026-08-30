#pragma once

#include "join_thread.h"
#include <algorithm>
#include <numeric>
#include <future>

template <typename Iterator>
void parallel_partial_sum(Iterator first, Iterator last) {
	using value_type = typename Iterator::value_type;

	struct process_chunk {
		void operator()(Iterator begin, Iterator last,
			std::future<value_type>* previous_end_value,
			std::promise<value_type>* end_value)
		{
			try
			{
				Iterator end = last;
				end++;
				std::partial_sum(begin, end, begin);
				if (previous_end_value) {
					value_type add_end = previous_end_value->get();
					*last += add_end;

					if (end_value) {
						end_value->set_value(*last);
					}

					std::for_each(begin, last, [add_end](value_type& item) {
						item += add_end;
					});
				}
				else if (end_value) {
					end_value->set_value(*last);
				}
			}
			catch (...)
			{
				if (end_value) {
					end_value->set_exception(std::current_exception());
				}
				else {
					throw;
				}
			}
		}
	};


	unsigned length = std::distance(first, last);
	if (!length) return;

	unsigned min_per_thread = 25;
	unsigned max_threads = (length + min_per_thread - 1) / min_per_thread;
	unsigned hardware_threads = std::thread::hardware_concurrency();

	unsigned num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
	unsigned block_size = length / num_threads;

	std::vector<std::thread> threads(num_threads - 1);
	std::vector<std::promise<value_type>> end_values(num_threads - 1);
	std::vector<std::future<value_type>> previous_end_values;
	previous_end_values.reserve(num_threads - 1);

	join_thread joiner(threads);
	Iterator block_start = first;
	for (unsigned i = 0; i < num_threads - 1; i++) {
		Iterator block_last = block_start;
		std::advance(block_last, block_size - 1);

		threads[i] = std::thread(process_chunk(), block_start, block_last,
			(i == 0) ? nullptr : &previous_end_values[i - 1], &end_values[i]);
		block_start = block_last;
		block_last++;

		previous_end_values.push_back(end_values[i].get_future());
	}

	Iterator final_element = block_start;
	std::advance(final_element, std::distance(block_start, last) - 1);
	process_chunk()(block_start, final_element, (num_threads > 1) ? &previous_end_values.back() : 0, 0);
}

