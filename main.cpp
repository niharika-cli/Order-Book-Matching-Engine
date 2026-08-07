#include "orderbook.hpp"
#include <iostream>

static void print_trades(const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "  TRADE: buy#" << t.buy_order_id << " x sell#"
                  << t.sell_order_id << "  " << t.quantity << " @ "
                  << (t.price / 10000.0) << "\n";
    }
}

static void print_top(const OrderBook& book) {
    auto bid = book.best_bid();
    auto ask = book.best_ask();
    std::cout << "  book: bid="
              << (bid ? std::to_string(*bid / 10000.0) : "none")
              << "  ask="
              << (ask ? std::to_string(*ask / 10000.0) : "none") << "\n";
}

int main() {
    OrderBook book;
    std::vector<Trade> trades;

    // Prices are fixed-point: dollars * 10000, so $100.50 == 1005000.
    auto px = [](double dollars) { return static_cast<int64_t>(dollars * 10000); };

    std::cout << "1. Resting liquidity: bid 100 @ $99.50, ask 100 @ $100.50\n";
    uint64_t bid1 = book.add_limit_order(Side::Buy, px(99.50), 100, trades);
    uint64_t ask1 = book.add_limit_order(Side::Sell, px(100.50), 100, trades);
    print_top(book);

    std::cout << "\n2. Second bid at same price (tests time priority): "
                 "50 @ $99.50\n";
    uint64_t bid2 = book.add_limit_order(Side::Buy, px(99.50), 50, trades);
    std::cout << "  quantity resting at $99.50: " << book.quantity_at(Side::Buy, px(99.50))
              << " (should be 150 = 100 + 50)\n";

    std::cout << "\n3. Aggressive sell crosses the spread: "
                 "sell 120 @ $99.00 (below best bid, so it should fill)\n";
    book.add_limit_order(Side::Sell, px(99.00), 120, trades);
    print_trades(trades);
    trades.clear();
    std::cout << "  (order #" << bid1 << " should fill first, then #" << bid2
              << " for the remainder — that's time priority)\n";
    print_top(book);

    std::cout << "\n4. Cancel the resting ask (#" << ask1 << ")\n";
    bool cancelled = book.cancel_order(ask1);
    std::cout << "  cancelled: " << (cancelled ? "yes" : "no") << "\n";
    print_top(book);

    std::cout << "\n5. Market buy for 30 — takes whatever's available, "
                 "no resting remainder if the book runs dry\n";
    book.add_market_order(Side::Buy, 30, trades);
    print_trades(trades);
    trades.clear();
    print_top(book);

    return 0;
}
