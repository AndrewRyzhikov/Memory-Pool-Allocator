#include <iostream>
#include <cmath>

template<typename T>
class PoolAllocator {
public:
    using value_type = T;
    using pointer = value_type*;

    constexpr PoolAllocator(const std::initializer_list<std::pair<size_t, size_t>>& list) : number_pools_(list.size()) {
        pools_ = new Pool[list.size()];

        size_t i = 0;
        for (auto [size_block, size_chunk]: list) {
            pools_[i++] = Pool(size_block, size_chunk);
        }
    }

    constexpr pointer allocate(const size_t size) {
        size_t size_value_type = sizeof(value_type);
        size_t free_size_suitable_pool = INT_MAX;
        size_t index_suitable_pool;

        for (size_t i = 0; i < number_pools_; ++i) {
            size_t free_size_current_pool = pools_[i].size_free_chunks_ * pools_[i].size_chunk_;
            if (size_value_type * size <= free_size_current_pool and free_size_current_pool < free_size_suitable_pool) {
                index_suitable_pool = i;
                free_size_suitable_pool = free_size_current_pool;
            }
        }

        if (free_size_suitable_pool != INT_MAX) {
            return reinterpret_cast<pointer>(pools_[index_suitable_pool].begin_chunk_ +
                                             pools_[index_suitable_pool].CheckPool(size_value_type * size) *
                                             pools_[index_suitable_pool].size_chunk_);
        } else {
            throw std::bad_alloc();
        }
    }

    constexpr void deallocate(pointer ptr, const size_t n) {
        bool found_target_pool = false;
        for (size_t i = 0; i < number_pools_; ++i) {
            if (pools_[i].FindChunk(ptr)) {
                found_target_pool = true;
                size_t index_ptr = (reinterpret_cast<char*>(ptr) - pools_[i].begin_chunk_) / pools_[i].size_chunk_;
                pools_[i].Deallocate(index_ptr, n);
            }
        }
        if (!found_target_pool) {
            throw std::invalid_argument("Pointer isn't valid");
        }
    }

    ~PoolAllocator() {
        std::destroy_n(pools_, number_pools_);
    }

private:
    struct Pool {
        size_t size_chunk_;
        size_t size_pool_;
        size_t size_free_chunks_;
        bool* free_ = nullptr;
        char* begin_chunk_ = nullptr;

        Pool() = default;

        Pool(size_t size_pool, size_t size_chunk) : size_chunk_(size_chunk), size_pool_(size_pool),
                                                    size_free_chunks_(size_pool) {
            begin_chunk_ = reinterpret_cast<char*>(malloc(size_pool * size_chunk));
            free_ = reinterpret_cast<bool*>(malloc(size_pool));

            for (size_t i = 0; i < size_pool; ++i) {
                free_[i] = true;
            }
        }

        bool FindChunk(pointer ptr) {
            return begin_chunk_ <= reinterpret_cast<char*>(ptr) and
                   reinterpret_cast<char*>(ptr) <= begin_chunk_ + size_chunk_ * (size_pool_ - 1);
        }

        void Deallocate(const size_t index_begin_chunk, const size_t n) {
            size_t size_delete_chunk = ceil(static_cast<double>(n * sizeof(value_type)) / size_chunk_);
            for (size_t i = index_begin_chunk; i < index_begin_chunk + size_delete_chunk - 1; ++i) {
                free_[i] = true;
            }
            size_free_chunks_ += size_delete_chunk;
        }

        size_t CheckPool(const size_t target_size) {
            size_t begin_free_size;
            bool current_segment = false;
            size_t current_size = 0;

            for (size_t i = 0; i < size_pool_; ++i) {
                if (free_[i]) {
                    current_size += size_chunk_;

                    if (!current_segment) {
                        current_segment = true;
                        begin_free_size = i;
                    }

                    if (current_size >= target_size) {
                        for (size_t j = begin_free_size; j <= i; ++j) {
                            free_[j] = false;
                        }

                        size_free_chunks_ -= (i - begin_free_size + 1);
                        return begin_free_size;
                    }
                } else {
                    current_segment = false;
                    current_size = 0;
                }
            }
            throw std::bad_alloc();
        }

        ~Pool() {
            free(free_);
            free(begin_chunk_);
        }
    };

    Pool* pools_;
    size_t number_pools_;
};
