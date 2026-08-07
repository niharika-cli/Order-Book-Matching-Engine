# orderbook

A single-instrument limit order book in C++17: price-time priority
matching, partial fills, cancels, and market (IOC) orders. The core
data structure and matching logic behind every exchange and every
market maker's internal book.

## The model

- **Price-time priority.** At a given price level, the order that
  arrived first fills first. Across price levels, better prices
  fill first (highest bid, lowest ask).
- **Price improvement.** When an aggressive order crosses the
  spread, it fills at the *resting* order's price, not its own limit
  — a buy willing to pay $10.10 that crosses a $10.00 ask pays
  $10.00, the same way a real exchange works.
- **Market orders are IOC** (immediate-or-cancel): they take
  whatever liquidity is available immediately, and whatever's left
  unfilled is discarded rather than resting in the book.

## Data structures

- `bids_`: `std::map<price, deque<Order>, greater<price>>` — highest
  price first.
- `asks_`: `std::map<price, deque<Order>>` — lowest price first.
- Each price level is a `deque`, so `front()` is always the earliest
  order at that price — matching against the front gives time
  priority for free, no extra bookkeeping needed.
- `index_`: order id → (side, price), so `cancel_order()` is O(log n)
  instead of scanning the whole book.

Deliberately **not thread-safe** — one book, one thread, matching
logic and locking don't mix well in the same class. If you need
concurrent access, put a queue in front of the book rather than
locking inside it.

## Build

```
g++ -std=c++17 -O2 orderbook.cpp main.cpp -o orderbook_demo
g++ -std=c++17 -O2 orderbook.cpp test_orderbook.cpp -o test_orderbook
```

## Run

```
./orderbook_demo
```

Walks through resting liquidity, a same-price second order (to show
time priority), a spread-crossing sell (to show matching and price
improvement), a cancel, and a market order — with commentary at each
step.

## Test

```
./test_orderbook
```

Covers: no-cross resting orders, exact-price full fills, price
improvement, time priority with partial fills across two orders at
the same level, cancel (including cancelling twice and cancelling a
nonexistent id), empty-level cleanup after the last order at a price
is removed, and market order behavior (fills available liquidity,
discards the rest, doesn't leave a resting order behind).

## Layout

| File                 | What it is                                    |
|----------------------|-------------------------------------------------|
| `orderbook.hpp`      | `Order`, `Trade`, `OrderBook` interface        |
| `orderbook.cpp`      | Matching logic, cancel, book queries           |
| `main.cpp`           | Narrated demo walking through a realistic sequence |
| `test_orderbook.cpp` | Correctness tests for matching and priority    |

## Natural extensions

- **Order modify** (cancel + re-add, but re-add should lose time
  priority — a common real-world rule worth implementing correctly).
- **Depth snapshot**: return the top N price levels on both sides
  for a market data feed, rather than just best bid/ask.
- **Throughput benchmark**: time `add_limit_order()` under a
  realistic order flow mix (mostly resting orders, occasional
  crosses) the same way `mdparse` benchmarks parsing — orders/sec
  and p99 latency per order.
