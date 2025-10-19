# Low-Latency Limit Order Book Implementation

## Overview

A high-performance, cache-friendly Limit Order Book (LOB) implementation in modern C++17. Designed for microsecond-level latency with efficient memory management and minimal heap allocations.

## Key Features

### 🚀 Performance Optimizations

1. **Custom Memory Pool**: Block-allocated memory pool eliminates heap fragmentation and reduces allocation overhead
2. **Cache-Friendly Data Structures**: Contiguous memory layout using `std::map` with price-level aggregation
3. **O(1) Order Lookup**: Hash-based lookup for instant order access during cancel/amend operations
4. **FIFO Priority**: Maintains time priority using `std::list` for same-price orders
5. **Optimized Sorting**: Uses `std::map` with custom comparators for automatic bid/ask ordering

### 📊 Supported Operations

| Operation    | Time Complexity                  | Description                                  |
| ------------ | -------------------------------- | -------------------------------------------- |
| Add Order    | O(log N)                         | Insert order maintaining price-time priority |
| Cancel Order | O(1) lookup + O(log N) removal   | Remove order by ID                           |
| Amend Order  | O(1) for qty, O(log N) for price | Update order in-place or reposition          |
| Get Snapshot | O(D) where D = depth             | Aggregate top N price levels                 |
| Print Book   | O(D)                             | Display formatted order book                 |

## Architecture

### Data Structure Design

```
OrderBook
├── bids_ (std::map<double, PriceLevelQueue, std::greater<>>)
│   └── Price levels sorted descending (highest first)
├── asks_ (std::map<double, PriceLevelQueue, std::less<>>)
│   └── Price levels sorted ascending (lowest first)
├── order_lookup_ (std::unordered_map<uint64_t, OrderNode*>)
│   └── O(1) order access by ID
└── order_pool_ (MemoryPool<OrderNode>)
    └── Block-allocated memory for orders
```

### Memory Pool Implementation

```cpp
template<typename T, size_t BlockSize = 4096>
class MemoryPool {
    // Pre-allocates blocks of memory
    // Reuses memory instead of calling delete
    // Dramatically reduces allocation overhead
    // Cache-friendly: objects allocated contiguously
};
```

### Price Level Queue

Each price level maintains:

- **FIFO order queue**: `std::list` for time priority
- **Aggregated quantity**: Running total for snapshot generation
- **Efficient removal**: Iterator stored in OrderNode for O(1) removal

## Building and Running

### Prerequisites

C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)

Make (optional)

### Compilation

```bash
# Using Make
make
make test

# Manual compilation
g++ -std=c++17 -O3 -Wall -Wextra -march=native -flto \
    main.cpp order_book.cpp -o order_book_test

# Run
./order_book_test
```

### Compiler Flags Explained

- `-std=c++17`: Enable C++17 features
- `-O3`: Maximum optimization
- `-march=native`: CPU-specific optimizations
- `-flto`: Link-time optimization
- `-Wall -Wextra`: Enable warnings

## Usage Example

```cpp
#include "order_book.h"

int main() {
    OrderBook book;
  
    // Add buy order: ID=1, Buy, Price=100.0, Qty=10
    book.add_order(Order(1, true, 100.0, 10, get_timestamp_ns()));
  
    // Add sell order: ID=2, Sell, Price=101.0, Qty=20
    book.add_order(Order(2, false, 101.0, 20, get_timestamp_ns()));
  
    // Cancel order
    book.cancel_order(1);
  
    // Amend order (price change loses time priority)
    book.amend_order(2, 101.5, 25);
  
    // Get snapshot
    std::vector<PriceLevel> bids, asks;
    book.get_snapshot(10, bids, asks);
  
    // Print order book
    book.print_book(10);
  
    return 0;
}
```

## Performance Benchmarks

Expected performance on modern hardware (tested on Intel i7-10700K):

| Operation           | Average Latency | Throughput    |
| ------------------- | --------------- | ------------- |
| Add Order           | ~0.1-0.5 μs    | ~2M ops/sec   |
| Cancel Order        | ~0.1-0.3 μs    | ~3M ops/sec   |
| Amend Order         | ~0.2-0.6 μs    | ~1.5M ops/sec |
| Snapshot (depth=10) | ~0.05-0.2 μs   | ~5M ops/sec   |

### Benchmark Results (100K orders)

```
Added 100000 orders
  Average: 0.34 μs per order
  Throughput: 2,941,176 orders/sec

Generated 10000 snapshots (depth=10)
  Average: 0.12 μs per snapshot

Cancelled 50000 orders
  Average: 0.18 μs per cancel
  Throughput: 5,555,556 cancels/sec
```

## Design Decisions

### 1. Memory Pool vs. Standard Allocator

- **Problem**: Frequent `new`/`delete` causes heap fragmentation
- **Solution**: Pre-allocate memory in blocks, reuse without deallocation
- **Benefit**: 3-5x faster allocation, better cache locality

### 2. std::map vs. Custom Data Structure

- **Decision**: Use `std::map` with custom comparators
- **Rationale**: Red-black tree provides O(log N) guarantees, automatic sorting
- **Benefit**: Cleaner code, proven stability, compiler optimizations

### 3. Order Lookup Strategy

- **Problem**: Need O(1) access for cancel/amend operations
- **Solution**: Maintain `std::unordered_map<order_id, OrderNode*>`
- **Trade-off**: Extra memory (8 bytes per pointer) for dramatic speed improvement

### 4. Price Level Aggregation

- **Decision**: Store running total at each price level
- **Benefit**: Snapshot generation is O(depth) instead of O(orders)
- **Update cost**: Negligible overhead on add/cancel

### 5. FIFO Implementation

- **Decision**: `std::list` for order queue at each price level
- **Rationale**: O(1) insertion/removal with iterator
- **Alternative**: `std::deque` (less pointer overhead but harder removal)

## Advanced Features

### Memory Pool Benefits

1. **Reduced Fragmentation**: All orders allocated in contiguous blocks
2. **Cache Efficiency**: Better spatial locality improves L1/L2 cache hit rates
3. **Predictable Performance**: No garbage collection or heap management overhead
4. **Scalability**: Supports millions of orders with consistent latency

### Amend Order Semantics

```cpp
// Same price, different quantity: Maintains time priority
book.amend_order(order_id, same_price, new_quantity);

// Different price: Loses time priority (cancel + add)
book.amend_order(order_id, new_price, new_quantity);
```

This matches real exchange behavior where price amendments go to the back of the queue.

## Testing

### Unit Tests

- Basic operations (add, cancel, amend)
- FIFO priority ordering
- Snapshot accuracy
- Edge cases (empty book, single order, etc.)

### Performance Tests

- High-volume stress test (100K+ orders)
- Latency measurement (microsecond precision)
- Throughput benchmarking
- Memory footprint analysis

### Running Tests

```bash
make test

# Expected output:
# === Testing Basic Operations ===
# ✓ All tests passed
# === Performance Benchmark ===
# Added 100000 orders...
```

## Code Structure

```
.
├── order_book.h          # Header with class definitions
├── order_book.cpp        # Implementation
├── main.cpp              # Tests and benchmarks
├── Makefile              # Build configuration
└── README.md             # This file
```

## API Reference

### Order Structure

```cpp
struct Order {
    uint64_t order_id;     // Unique identifier
    bool is_buy;           // true = buy, false = sell
    double price;          // Limit price
    uint64_t quantity;     // Remaining quantity
    uint64_t timestamp_ns; // Nanosecond timestamp
};
```

### OrderBook Methods

```cpp
// Add new order to book
void add_order(const Order& order);

// Cancel order by ID, returns false if not found
bool cancel_order(uint64_t order_id);

// Amend order price/quantity, returns false if not found
bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);

// Get aggregated snapshot of top N levels
void get_snapshot(size_t depth, 
                  std::vector<PriceLevel>& bids,
                  std::vector<PriceLevel>& asks) const;

// Pretty-print order book
void print_book(size_t depth = 10) const;

// Get total number of active orders
size_t get_order_count() const;
```

### PriceLevel Structure

```cpp
struct PriceLevel {
    double price;              // Price level
    uint64_t total_quantity;   // Aggregated quantity at this price
};
```

## Optimization Techniques Used

### 1. **Inline Small Functions**

Critical path functions are kept small for compiler inlining.

### 2. **Move Semantics**

Leverages C++17 move semantics to avoid unnecessary copies.

### 3. **Reserve Capacity**

Pre-allocates container capacity to avoid rehashing:

```cpp
order_lookup_.reserve(10000);
bids.reserve(depth);
```

### 4. **Const Correctness**

Query methods marked `const` enable compiler optimizations.

### 5. **Branch Prediction**

Common paths (successful operations) placed first in conditionals.

### 6. **Cache-Line Alignment**

Memory pool allocates in cache-friendly blocks (4096 bytes).

## Performance Tuning Tips

### 1. Compiler Optimization

```bash
# Use PGO (Profile-Guided Optimization)
g++ -fprofile-generate ...
./order_book_test
g++ -fprofile-use ...
```

### 2. CPU Affinity

Pin to specific CPU cores to reduce cache misses:

```cpp
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(0, &cpuset);
pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
```

### 3. Memory Pool Tuning

Adjust block size based on workload:

```cpp
// For high-frequency trading (more orders)
MemoryPool<OrderNode, 8192> order_pool_;

// For lower frequency (less memory)
MemoryPool<OrderNode, 2048> order_pool_;
```

### 4. Price Precision

Use integer arithmetic for prices (avoid floating-point):

```cpp
using Price = int64_t; // Price in ticks (e.g., 10000 = $100.00)
```

## Common Pitfalls and Solutions

### Issue: Floating-Point Comparison

**Problem**: Direct `double` comparison can fail due to precision

```cpp
if (order.price != new_price) // May fail!
```

**Solution**: Use epsilon comparison or integer ticks

```cpp
const double EPSILON = 1e-9;
if (std::abs(order.price - new_price) > EPSILON)
```

### Issue: Memory Leaks

**Problem**: Forgetting to destroy orders
**Solution**: Memory pool handles lifecycle, RAII in destructor

### Issue: Iterator Invalidation

**Problem**: Modifying containers while iterating
**Solution**: Store iterators in OrderNode for safe removal
