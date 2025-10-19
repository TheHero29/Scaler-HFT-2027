#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <list>
#include <memory>
#include <iostream>
#include <iomanip>

// Order structure
struct Order {
    uint64_t order_id;
    bool is_buy;
    double price;
    uint64_t quantity;
    uint64_t timestamp_ns;
    
    Order(uint64_t id, bool buy, double p, uint64_t q, uint64_t ts)
        : order_id(id), is_buy(buy), price(p), quantity(q), timestamp_ns(ts) {}
};

// Price level aggregation
struct PriceLevel {
    double price;
    uint64_t total_quantity;
    
    PriceLevel(double p = 0.0, uint64_t q = 0)
        : price(p), total_quantity(q) {}
};

// Memory pool for efficient order allocation
template<typename T, size_t BlockSize = 4096>
class MemoryPool {
public:
    MemoryPool() : current_block_(nullptr), current_slot_(0) {
        allocate_block();
    }
    
    ~MemoryPool() {
        for (auto block : blocks_) {
            ::operator delete(block);
        }
    }
    
    template<typename... Args>
    T* construct(Args&&... args) {
        if (current_slot_ >= BlockSize) {
            allocate_block();
        }
        
        T* ptr = &current_block_[current_slot_++];
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }
    
    void destroy(T* ptr) {
        ptr->~T();
        // Note: We don't actually free memory, it's reused in the pool
    }
    
private:
    void allocate_block() {
        T* new_block = static_cast<T*>(::operator new(BlockSize * sizeof(T)));
        blocks_.push_back(new_block);
        current_block_ = new_block;
        current_slot_ = 0;
    }
    
    std::vector<T*> blocks_;
    T* current_block_;
    size_t current_slot_;
};

// Internal order node with list iterator
struct OrderNode {
    Order order;
    std::list<OrderNode*>::iterator list_iter;
    
    OrderNode(const Order& o) : order(o) {}
};

// Price level implementation with FIFO queue
class PriceLevelQueue {
public:
    void add_order(OrderNode* node) {
        orders_.push_back(node);
        node->list_iter = std::prev(orders_.end());
        total_quantity_ += node->order.quantity;
    }
    
    void remove_order(OrderNode* node) {
        total_quantity_ -= node->order.quantity;
        orders_.erase(node->list_iter);
    }
    
    void update_quantity(OrderNode*, uint64_t old_qty, uint64_t new_qty) {
        total_quantity_ = total_quantity_ - old_qty + new_qty;
    }
    
    bool is_empty() const {
        return orders_.empty();
    }
    
    uint64_t get_total_quantity() const {
        return total_quantity_;
    }
    
    const std::list<OrderNode*>& get_orders() const {
        return orders_;
    }
    
private:
    std::list<OrderNode*> orders_;
    uint64_t total_quantity_ = 0;
};

class OrderBook {
public:
    OrderBook();
    ~OrderBook();
    
    // Core operations
    void add_order(const Order& order);
    bool cancel_order(uint64_t order_id);
    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);
    
    // Query operations
    void get_snapshot(size_t depth, std::vector<PriceLevel>& bids, 
                     std::vector<PriceLevel>& asks) const;
    void print_book(size_t depth = 10) const;
    
    // Statistics
    size_t get_order_count() const { return order_lookup_.size(); }
    
private:
    // Internal helper methods
    void add_to_side(OrderNode* node);
    void remove_from_side(OrderNode* node);
    PriceLevelQueue* get_or_create_level(double price, bool is_buy);
    void remove_level_if_empty(double price, bool is_buy);
    
    // Buy side: descending order (highest price first)
    std::map<double, PriceLevelQueue, std::greater<double>> bids_;
    
    // Sell side: ascending order (lowest price first)
    std::map<double, PriceLevelQueue, std::less<double>> asks_;
    
    // Fast O(1) order lookup
    std::unordered_map<uint64_t, OrderNode*> order_lookup_;
    
    // Memory pool for efficient allocation
    MemoryPool<OrderNode, 4096> order_pool_;
};