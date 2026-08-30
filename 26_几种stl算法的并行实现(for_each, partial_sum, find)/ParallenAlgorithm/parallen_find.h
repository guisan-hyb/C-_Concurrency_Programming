#pragma once

#include "join_thread.h"
#include <future>

template <typename Iterator, typename MatchType>
Iterator parallen_find(Iterator first, Iterator last, MatchType match) {
	struct find_element {
		void operator()(Iterator begin, Iterator end, MatchType match,
			std::promise<Iterator>& result, std::atomic<bool>& done_flag) 
		{
			try {
				for (; (begin != end) && !done_flag.load(); ++begin) {
					if (*begin == match) {
						bool expected = false;
						if (done_flag.compare_exchange_strong(expected, true)) {
							// 只有第一个成功将 false 改为 true 的线程，才会进入这里
							// 因此 set_value 绝对不会被调用第二次，不会抛出异常
							result.set_value(begin);
						}
						return;// 无论是哪个线程触发了 done_flag，当前线程都应该结束查找了
					}
				}
			}
			catch (...) {
				try {
					result.set_exception(std::current_exception());
					done_flag.store(true);
				}
				catch (...) {

				}
			}
		}
	};


	unsigned lenth = std::distance(first, last);
	if (!lenth) return last;

	unsigned min_per_thread = 25;
	unsigned max_threads = (lenth + min_per_thread - 1) / min_per_thread;
	unsigned hardware_threads = std::thread::hardware_concurrency();
	
	unsigned num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
	unsigned block_size = lenth / num_threads;

	std::promise<Iterator> result;
	std::atomic<bool> done_flag(false);
	std::vector<std::thread> threads(num_threads - 1);

	{
		join_thread joiner(threads);
		Iterator block_start = first;
		for (unsigned i = 0; i < num_threads - 1; i++) {
			Iterator block_end = block_start;
			std::advance(block_end, block_size);

			threads[i] = std::thread(find_element(), block_start, block_end, match,
				std::ref(result), std::ref(done_flag));

			block_start = block_end;
		}

		// 主线程可以直接传引用，不需要 std::ref
		find_element()(block_start, last, match, result, done_flag);
	}

	if (!done_flag.load()) return last;

	return result.get_future().get();
}





template <typename Iterator, typename MatchType>
Iterator parallel_find_impl(Iterator first, Iterator last, MatchType match, std::atomic<bool>& done) {
	try {
		unsigned lenth = std::distance(first, last);
		unsigned min_per_thread = 25;
		if (lenth < (2 * min_per_thread)) {
			for (; (first != last) && !done.load(); first++) {
				if (*first == match) {
					bool expected = false;
					if (done.compare_exchange_strong(expected, true)) {
						return first;
					}
				}
			}
			return last;
		}
		else {
			Iterator mid = first + lenth / 2;
			std::future<Iterator> async_result = std::async(&parallel_find_impl<Iterator, MatchType>,
				mid, last, match, std::ref(done));
			Iterator direct_result = parallel_find_impl(first, mid, match, std::ref(done));
			return (direct_result == mid) ? async_result.get() : direct_result;
		}
	}
	catch (...) {
		done.store(true);
		throw;
	}
}

template <typename Iterator, typename MatchType>
Iterator parallel_find_async(Iterator first, Iterator last, MatchType match) {
	std::atomic<bool> done(false);
	return parallel_find_impl(first, last, match, done);
}

