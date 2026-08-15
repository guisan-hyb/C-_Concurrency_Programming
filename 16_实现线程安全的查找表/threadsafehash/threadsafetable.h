#pragma once

#include <thread>
#include <list>
#include <map>
#include <memory>
#include <shared_mutex>
#include <iterator>
#include <vector>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class threadsafetable {
private:
	class bucket_type {
		friend class threadsafetable;
	private:
		using bucket_value = std::pair<Key, Value>;
		using bucket_data = std::list<bucket_value>;
		using bucket_iterator = typename bucket_data::iterator;//注意这里要写typename指明它是个类型

		bucket_data data;
		mutable std::shared_mutex mutex;

	private:
		bucket_iterator find_entry_for(const Key& key) {
			return std::find_if(data.begin(), data.end(), [&](const bucket_value& item) {
				return item.first == key;
			});
		}

	public:
		//查找Key值
		Value value_for(const Key& key, const Value& default_val) {
			std::shared_lock<std::shared_mutex> lk(mutex);
			const bucket_iterator found_entry = find_entry_for(key);

			return found_entry == data.end() ? default_val : found_entry->second;
		}

		//添加/更新key和val
		void add_or_update_mapping(const Key& key, const Value& val) {
			std::unique_lock<std::shared_mutex> lk(mutex);
			auto found_entry = find_entry_for(key);
			if (found_entry == data.end()) {
				data.push_back(bucket_value(key, val));
			}
			else {
				found_entry->second = val;
			}
		}

		//删除key
		void remove_mapping(const Key& key) {
			std::unique_lock<std::shared_mutex> lk(mutex);
			auto found_entry = find_entry_for(key);
			if (found_entry != data.end()) {
				data.erase(found_entry);
			}
		}
	};

private:
	std::vector<std::unique_ptr<bucket_type>> buckets;
	Hash hasher;

	bucket_type& get_bucket(const Key& key) const {
		std::size_t bucket_index = hasher(key) % buckets.size();
		return *(buckets[bucket_index]);
	}

public:
	threadsafetable(unsigned num_buckets = 19, const Hash& hasher_ = Hash())
		: buckets(num_buckets), hasher(hasher_) {
		for (unsigned i = 0; i < num_buckets; i++) {
			buckets[i].reset(new bucket_type);
		}
	}

	threadsafetable(const threadsafetable&) = delete;
	threadsafetable& operator=(const threadsafetable&) = delete;

	Value value_for(const Key& key, const Value& default_val = Value()) {
		return get_bucket(key).value_for(key, default_val);
	}

	void add_or_update_mapping(const Key& key, const Value& val) {
		return get_bucket(key).add_or_update_mapping(key, val);
	}

	void remove_mapping(const Key& key) {
		return get_bucket(key).remove_mapping(key);
	}

	std::map<Key, Value> get_map() {
		std::vector<std::unique_lock<std::shared_mutex>> locks;
		for (unsigned i = 0; i < buckets.size(); i++) {
			locks.push_back(std::unique_lock<std::shared_mutex>(buckets[i]->mutex));
		}
		std::map<Key, Value> ret;
		for (unsigned i = 0; i < buckets.size(); i++) {
			auto it = buckets[i]->data.begin();
			for (; it != buckets[i]->data.end(); it++) {
				ret.insert(*it);
			}
		}

		return ret;
	}
};