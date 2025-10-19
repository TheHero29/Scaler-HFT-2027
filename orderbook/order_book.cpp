#include "order_book.h"
#include <algorithm>
#include <stdexcept>

OrderBook::OrderBook() {
    // Reserve space to minimize rehashing
    order_lookup_.reserve(10000);
}

OrderBook::~OrderBook() {
    // Clean up all order nodes
    for (auto& [order_id, node] : order_lookup_) {
        order_pool_.destroy(node);
    }
}

void OrderBook::add_order(const Order& order) {
    // Check if order already exists
    if (order_lookup_.find(order.order_id) != order_lookup_.end()) {
        return; // Duplicate order ID
    }
    
    // Allocate from pool (cache-friendly, no heap fragmentation)
    OrderNode* node = order_pool_.construct(order);
    
    // Add to appropriate side
    add_to_side(node);
    
    // Add to lookup table for O(1) access
    order_lookup_[order.order_id] = node;
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        return false;
    }
    
    OrderNode* node = it->second;
    
    // Remove from price level
    remove_from_side(node);
    
    // Remove from lookup
    order_lookup_.erase(it);
    
    // Return to pool
    order_pool_.destroy(node);
    
    return true;
}

bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        return false;
    }
    
    OrderNode* node = it->second;
    Order& order = node->order;
    
    // If price changes, treat as cancel + add (loses time priority)
    if (order.price != new_price) {
        // Remove from old price level
        remove_from_side(node);
        
        // Update order details
        order.price = new_price;
        order.quantity = new_quantity;
        
        // Add to new price level (goes to back of queue)
        add_to_side(node);
        
        return true;
    }
    
    // Only quantity changed - update in place (maintains time priority)
    if (order.quantity != new_quantity) {
        if (order.is_buy) {
            auto level_it = bids_.find(order.price);
            if (level_it != bids_.end()) {
                uint64_t old_qty = order.quantity;
                order.quantity = new_quantity;
                level_it->second.update_quantity(nullptr, old_qty, new_quantity);
                return true;
            }
        } else {
            auto level_it = asks_.find(order.price);
            if (level_it != asks_.end()) {
                uint64_t old_qty = order.quantity;
                order.quantity = new_quantity;
                level_it->second.update_quantity(nullptr, old_qty, new_quantity);
                return true;
            }
        }
    }
    
    // No changes made (price and quantity are the same)
    return true;
}

void OrderBook::get_snapshot(size_t depth, std::vector<PriceLevel>& bids,
                             std::vector<PriceLevel>& asks) const {
    bids.clear();
    asks.clear();
    
    // Reserve space to avoid reallocation
    bids.reserve(depth);
    asks.reserve(depth);
    
    // Get top N bids (already sorted descending)
    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        if (count >= depth) break;
        bids.emplace_back(price, level.get_total_quantity());
        ++count;
    }
    
    // Get top N asks (already sorted ascending)
    count = 0;
    for (const auto& [price, level] : asks_) {
        if (count >= depth) break;
        asks.emplace_back(price, level.get_total_quantity());
        ++count;
    }
}

void OrderBook::print_book(size_t depth) const {
    std::vector<PriceLevel> bids, asks;
    get_snapshot(depth, bids, asks);
    
    std::cout << "\n========== ORDER BOOK ==========\n";
    std::cout << std::fixed << std::setprecision(2);
    
    // Print asks in reverse (highest first)
    std::cout << "\n--- ASKS ---\n";
    std::cout << std::setw(12) << "Price" << " | " 
              << std::setw(12) << "Quantity" << "\n";
    std::cout << std::string(28, '-') << "\n";
    
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << std::setw(12) << it->price << " | "
                  << std::setw(12) << it->total_quantity << "\n";
    }
    
    std::cout << std::string(28, '=') << "\n";
    
    // Print bids (highest first)
    std::cout << "--- BIDS ---\n";
    std::cout << std::setw(12) << "Price" << " | " 
              << std::setw(12) << "Quantity" << "\n";
    std::cout << std::string(28, '-') << "\n";
    
    for (const auto& bid : bids) {
        std::cout << std::setw(12) << bid.price << " | "
                  << std::setw(12) << bid.total_quantity << "\n";
    }
    
    std::cout << "================================\n\n";
}

// Private helper methods

void OrderBook::add_to_side(OrderNode* node) {
    double price = node->order.price;
    
    if (node->order.is_buy) {
        // Get or create price level for bids
        auto& level = bids_[price];
        level.add_order(node);
    } else {
        // Get or create price level for asks
        auto& level = asks_[price];
        level.add_order(node);
    }
}

void OrderBook::remove_from_side(OrderNode* node) {
    double price = node->order.price;
    
    if (node->order.is_buy) {
        // Remove from bids
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            it->second.remove_order(node);
            
            // Remove empty price levels
            if (it->second.is_empty()) {
                bids_.erase(it);
            }
        }
    } else {
        // Remove from asks
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            it->second.remove_order(node);
            
            // Remove empty price levels
            if (it->second.is_empty()) {
                asks_.erase(it);
            }
        }
    }
}