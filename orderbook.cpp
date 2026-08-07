#include "orderbook.hpp"
#include <algorithm>

namespace {
// Fills `incoming` against price levels in `book`, front of each
// level first (time priority), for as long as `crosses(level_price)`
// says the level is marketable against the incoming order. Removes
// resting orders as they're fully filled, removes price levels as
// they empty, and appends every fill to `trades_out`.
template <typename BookMap, typename CrossFn>
void fill_against(Order& incoming, BookMap& book, CrossFn crosses,
                   std::unordered_map<uint64_t, std::pair<Side, int64_t>>& index,
                   std::vector<Trade>& trades_out) {
    while (incoming.quantity > 0 && !book.empty()) {
        auto level_it = book.begin();
        int64_t level_price = level_it->first;
        if (!crosses(level_price)) break;

        auto& level = level_it->second;
        while (incoming.quantity > 0 && !level.empty()) {
            Order& resting = level.front();
            uint32_t fill_qty = std::min(incoming.quantity, resting.quantity);

            // Trade price is always the RESTING order's price — the
            // side that was already in the book. The aggressor (the
            // incoming order) gets "price improvement": a buy that
            // crossed at $101 while the best ask sits at $100 pays
            // $100, not $101.
            if (incoming.side == Side::Buy) {
                trades_out.push_back({incoming.id, resting.id, level_price, fill_qty});
            } else {
                trades_out.push_back({resting.id, incoming.id, level_price, fill_qty});
            }

            incoming.quantity -= fill_qty;
            resting.quantity -= fill_qty;
            if (resting.quantity == 0) {
                index.erase(resting.id);
                level.pop_front();
            }
        }
        if (level.empty()) book.erase(level_it);
    }
}
}  // namespace

void OrderBook::match(Order& incoming, std::vector<Trade>& trades_out) {
    if (incoming.side == Side::Buy) {
        // A buy crosses any ask priced at or below what it's willing to pay.
        fill_against(incoming, asks_,
                     [&](int64_t ask_price) { return ask_price <= incoming.price; },
                     index_, trades_out);
    } else {
        // A sell crosses any bid priced at or above what it's willing to accept.
        fill_against(incoming, bids_,
                     [&](int64_t bid_price) { return bid_price >= incoming.price; },
                     index_, trades_out);
    }
}

uint64_t OrderBook::add_limit_order(Side side, int64_t price, uint32_t quantity,
                                     std::vector<Trade>& trades_out) {
    Order incoming{next_id_++, side, price, quantity};
    uint64_t id = incoming.id;

    match(incoming, trades_out);

    // Whatever didn't fill rests in the book at the order's own limit price.
    if (incoming.quantity > 0) {
        if (side == Side::Buy) {
            bids_[price].push_back(incoming);
        } else {
            asks_[price].push_back(incoming);
        }
        index_[id] = {side, price};
    }
    return id;
}

void OrderBook::add_market_order(Side side, uint32_t quantity,
                                  std::vector<Trade>& trades_out) {
    // A market order crosses at any price, so `crosses` always
    // returns true — it eats through levels until either it's fully
    // filled or the book runs out of that side entirely. Whatever's
    // left unfilled is discarded, not rested (this is what makes it
    // a market order rather than an aggressively-priced limit order).
    Order incoming{next_id_++, side, 0, quantity};
    if (side == Side::Buy) {
        fill_against(incoming, asks_, [](int64_t) { return true; }, index_, trades_out);
    } else {
        fill_against(incoming, bids_, [](int64_t) { return true; }, index_, trades_out);
    }
}

bool OrderBook::cancel_order(uint64_t id) {
    auto idx_it = index_.find(id);
    if (idx_it == index_.end()) return false;

    Side side = idx_it->second.first;
    int64_t price = idx_it->second.second;

    // bids_ and asks_ are different types (different map comparator),
    // so this can't be a single `auto&` ternary — branch instead.
    auto erase_from = [&](auto& book) {
        auto level_it = book.find(price);
        if (level_it == book.end()) return false;  // shouldn't happen if index_ is consistent

        auto& level = level_it->second;
        auto order_it = std::find_if(level.begin(), level.end(),
                                      [&](const Order& o) { return o.id == id; });
        if (order_it == level.end()) return false;

        level.erase(order_it);
        if (level.empty()) book.erase(level_it);
        return true;
    };

    bool erased = (side == Side::Buy) ? erase_from(bids_) : erase_from(asks_);
    if (!erased) return false;

    index_.erase(idx_it);
    return true;
}

std::optional<int64_t> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<int64_t> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

uint32_t OrderBook::quantity_at(Side side, int64_t price) const {
    auto sum_level = [&](const auto& book) {
        auto it = book.find(price);
        if (it == book.end()) return 0u;
        uint32_t total = 0;
        for (const auto& order : it->second) total += order.quantity;
        return total;
    };
    return (side == Side::Buy) ? sum_level(bids_) : sum_level(asks_);
}
