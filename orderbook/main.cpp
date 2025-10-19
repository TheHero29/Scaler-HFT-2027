#include "order_book.h"
#include <chrono>
#include <random>
#include <algorithm>
#include <cassert>
#include <iostream>

// High-resolution timer
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_us() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start_).count();
    }
    
    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Get current timestamp in nanoseconds
uint64_t get_timestamp_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Test basic functionality
void test_basic_operations() {
    std::cout << "=== Testing Basic Operations ===\n";
    
    OrderBook book;
    
    // Add orders
    book.add_order(Order(1, true, 100.0, 10, get_timestamp_ns()));
    book.add_order(Order(2, true, 100.0, 20, get_timestamp_ns()));
    book.add_order(Order(3, true, 99.5, 15, get_timestamp_ns()));
    book.add_order(Order(4, false, 101.0, 25, get_timestamp_ns()));
    book.add_order(Order(5, false, 101.5, 30, get_timestamp_ns()));
    
    book.print_book(5);
    
    assert(book.get_order_count() == 5);
    std::cout << "✓ Add orders test passed\n";
    
    // Test snapshot
    std::vector<PriceLevel> bids, asks;
    book.get_snapshot(2, bids, asks);
    
    assert(bids.size() == 2);
    assert(asks.size() == 2);
    assert(bids[0].price == 100.0);
    assert(bids[0].total_quantity == 30); // 10 + 20
    assert(bids[1].price == 99.5);
    assert(bids[1].total_quantity == 15);
    std::cout << "✓ Snapshot test passed\n";
    
    // Cancel order
    bool cancelled = book.cancel_order(2);
    assert(cancelled);
    assert(book.get_order_count() == 4);
    
    book.get_snapshot(2, bids, asks);
    assert(bids[0].total_quantity == 10); // Only order 1 remains at 100.0
    std::cout << "✓ Cancel order test passed\n";
    
    // Amend quantity (same price)
    bool amended = book.amend_order(1, 100.0, 50);
    assert(amended);
    
    book.get_snapshot(2, bids, asks);
    assert(bids[0].total_quantity == 50);
    std::cout << "✓ Amend quantity test passed\n";
    
    // Amend price (loses time priority)
    amended = book.amend_order(1, 99.0, 50);
    assert(amended);
    
    book.get_snapshot(3, bids, asks);
    assert(bids[0].price == 99.5);
    assert(bids[1].price == 99.0);
    std::cout << "✓ Amend price test passed\n";
    
    book.print_book(5);
    
    std::cout << "\n✅ All basic tests passed!\n\n";
}

// Benchmark performance
void benchmark_performance() {
    std::cout << "=== Performance Benchmark ===\n";
    
    OrderBook book;
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> price_dist(90.0, 110.0);
    std::uniform_int_distribution<uint64_t> qty_dist(1, 1000);
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    const size_t num_orders = 100000;
    std::vector<uint64_t> order_ids;
    order_ids.reserve(num_orders);
    
    // Benchmark: Add orders
    Timer timer;
    for (size_t i = 0; i < num_orders; ++i) {
        Order order(
            i + 1,
            side_dist(rng) == 1,
            std::round(price_dist(rng) * 100.0) / 100.0, // Round to 2 decimals
            qty_dist(rng),
            get_timestamp_ns()
        );
        book.add_order(order);
        order_ids.push_back(order.order_id);
    }
    double add_time = timer.elapsed_us();
    double avg_add_time = add_time / num_orders;
    
    std::cout << "Added " << num_orders << " orders\n";
    std::cout << "  Total time: " << add_time / 1000.0 << " ms\n";
    std::cout << "  Average: " << avg_add_time << " μs per order\n";
    std::cout << "  Throughput: " << (num_orders / (add_time / 1e6)) << " orders/sec\n\n";
    
    // Benchmark: Snapshots
    timer.reset();
    const size_t num_snapshots = 10000;
    std::vector<PriceLevel> bids, asks;
    
    for (size_t i = 0; i < num_snapshots; ++i) {
        book.get_snapshot(10, bids, asks);
    }
    double snapshot_time = timer.elapsed_us();
    double avg_snapshot_time = snapshot_time / num_snapshots;
    
    std::cout << "Generated " << num_snapshots << " snapshots (depth=10)\n";
    std::cout << "  Total time: " << snapshot_time / 1000.0 << " ms\n";
    std::cout << "  Average: " << avg_snapshot_time << " μs per snapshot\n\n";
    
    // Benchmark: Cancellations
    // Shuffle the order IDs to test random cancellations
    std::shuffle(order_ids.begin(), order_ids.end(), rng);
    const size_t num_cancels = std::min(num_orders / 2, (size_t)50000);
    
    timer.reset();
    for (size_t i = 0; i < num_cancels; ++i) {
        book.cancel_order(order_ids[i]);
    }
    double cancel_time = timer.elapsed_us();
    double avg_cancel_time = cancel_time / num_cancels;
    
    std::cout << "Cancelled " << num_cancels << " orders\n";
    std::cout << "  Total time: " << cancel_time / 1000.0 << " ms\n";
    std::cout << "  Average: " << avg_cancel_time << " μs per cancel\n";
    std::cout << "  Throughput: " << (num_cancels / (cancel_time / 1e6)) << " cancels/sec\n\n";
    
    // Benchmark: Amendments
    std::vector<uint64_t> remaining_ids;
    for (size_t i = num_cancels; i < order_ids.size(); ++i) {
        remaining_ids.push_back(order_ids[i]);
    }
    
    const size_t num_amends = std::min(remaining_ids.size(), (size_t)10000);
    timer.reset();
    
    for (size_t i = 0; i < num_amends; ++i) {
        double new_price = std::round(price_dist(rng) * 100.0) / 100.0;
        uint64_t new_qty = qty_dist(rng);
        book.amend_order(remaining_ids[i], new_price, new_qty);
    }
    double amend_time = timer.elapsed_us();
    double avg_amend_time = amend_time / num_amends;
    
    std::cout << "Amended " << num_amends << " orders\n";
    std::cout << "  Total time: " << amend_time / 1000.0 << " ms\n";
    std::cout << "  Average: " << avg_amend_time << " μs per amend\n";
    std::cout << "  Throughput: " << (num_amends / (amend_time / 1e6)) << " amends/sec\n\n";
    
    std::cout << "Final book state:\n";
    std::cout << "  Active orders: " << book.get_order_count() << "\n\n";
    
    book.print_book(5);
}

// Test FIFO ordering
void test_fifo_priority() {
    std::cout << "=== Testing FIFO Priority ===\n";
    
    OrderBook book;
    
    // Add multiple orders at same price
    book.add_order(Order(1, true, 100.0, 10, get_timestamp_ns()));
    book.add_order(Order(2, true, 100.0, 20, get_timestamp_ns()));
    book.add_order(Order(3, true, 100.0, 30, get_timestamp_ns()));
    
    std::vector<PriceLevel> bids, asks;
    book.get_snapshot(1, bids, asks);
    
    assert(bids[0].total_quantity == 60); // All orders aggregated
    std::cout << "✓ FIFO aggregation test passed\n";
    
    // Cancel middle order
    book.cancel_order(2);
    book.get_snapshot(1, bids, asks);
    assert(bids[0].total_quantity == 40);
    std::cout << "✓ FIFO cancellation test passed\n";
    
    std::cout << "\n✅ FIFO priority tests passed!\n\n";
}

int main() {
    std::cout << "\n╔════════════════════════════════════════╗\n";
    std::cout << "║  Low-Latency Limit Order Book (C++17)  ║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";
    
    try {
        test_basic_operations();
        test_fifo_priority();
        benchmark_performance();
        
        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║          All Tests Passed! ✓           ║\n";
        std::cout << "╚════════════════════════════════════════╝\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}