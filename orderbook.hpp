#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

enum class Side { Buy, Sell };

struct Order {
    uint64_t id;
    Side side;
    int64_t price;      // fixed-point: real price * 10000, so orders compare as plain integers
    uint32_t quantity;   // remaining quantity — shrinks as the order fills
};

struct Trade {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    int64_t price;       // always the resting order's price — the aggressor gets price improvement
    uint32_t quantity;
};

// A single-instrument limit order book with price-time priority
// matching. Not thread-safe by design — one book, one thread. If you
// need concurrent access, put a queue in front of it rather than
// locking the book itself; matching logic and locking don't mix well.
class OrderBook {
public:
    // Submits a limit order. Matches immediately against the
    // opposite side while prices cross; any unfilled remainder rests
    // in the book. Appends every resulting fill to `trades_out` (does
    // not clear it first, so callers can accumulate across calls).
    // Returns the new order's id.
    uint64_t add_limit_order(Side side, int64_t price, uint32_t quantity,
                              std::vector<Trade>& trades_out);

    // Submits a market order: matches immediately against whatever
    // liquidity is available (crossing at the resting orders'
    // prices), but any unfilled remainder is discarded rather than
    // resting in the book (an "immediate-or-cancel" order).
    void add_market_order(Side side, uint32_t quantity,
                           std::vector<Trade>& trades_out);

    // Removes a resting order. Returns false if the id doesn't exist
    // (already filled, already cancelled, or never existed).
    bool cancel_order(uint64_t id);

    std::optional<int64_t> best_bid() const;
    std::optional<int64_t> best_ask() const;

    // Total resting quantity at a specific price level on a specific
    // side. Zero if the level doesn't exist. Useful for tests and for
    // building a depth display.
    uint32_t quantity_at(Side side, int64_t price) const;

    size_t order_count() const { return index_.size(); }

private:
    void match(Order& incoming, std::vector<Trade>& trades_out);

    uint64_t next_id_ = 1;

    // Bids sorted highest price first (best bid = begin()); asks
    // sorted lowest price first (best ask = begin()). Each price
    // level is a deque so earlier orders sit at the front — matching
    // always fills level.front() first, which is what gives
    // time priority within a price level.
    std::map<int64_t, std::deque<Order>, std::greater<int64_t>> bids_;
    std::map<int64_t, std::deque<Order>> asks_;

    // order id -> (side, price), so cancel() doesn't have to scan the
    // whole book to find which level an order lives on.
    std::unordered_map<uint64_t, std::pair<Side, int64_t>> index_;
};
