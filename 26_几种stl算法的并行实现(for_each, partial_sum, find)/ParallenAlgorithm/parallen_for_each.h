#pragma once

#include <thread>
#include <future>
#include <algorithm>
#include "join_thread.h"

template <typename Iterator, typename Func>
void parallen_for_each(Iterator first, Iterator last, Func f) {
	unsigned length = std::distance(first, last);
	if (!length) return;

	unsigned min_per_thread = 25;
	unsigned max_threads = (length + min_per_thread - 1) / min_per_thread;
	unsigned hardware_threads = std::thread::hardware_concurrency();

	unsigned num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
	unsigned block_size = length / num_threads;

	std::vector<std::thread> threads(num_threads - 1);
	std::vector<std::future<void>> futures(num_threads - 1);
	join_thread joiner(threads);
	Iterator block_start = first;
	for (unsigned i = 0; i < num_threads - 1; i++) {
		Iterator block_end = block_start;
		std::advance(block_end, block_size);

		std::packaged_task<void(void)> task([=]() {
			std::for_each(block_start, block_end, f);
		});

		futures[i] = task.get_future();
		threads[i] = std::thread(std::move(task));

		block_start = block_end;
	}
	std::for_each(block_start, last, f);
	for (unsigned i = 0; i < num_threads - 1; i++) {
		futures[i].get();
	}
}


template <typename Iterator, typename Func>
void async_for_each(Iterator first, Iterator last, Func f) {
	unsigned length = std::distance(first, last);
	if (!length) return;
	unsigned min_per_thread = 25;

	if (length < 2 * min_per_thread) {
		std::for_each(first, last, f);
	}
	else {
		Iterator mid = first + length / 2;
		std::future<void> first_half = std::async(&async_for_each<Iterator, Func>,
			first, mid, f);
		parallen_for_each(mid, last, f);
		first_half.get();
	}
}

