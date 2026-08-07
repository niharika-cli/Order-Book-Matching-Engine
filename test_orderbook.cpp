#include "orderbook.hpp"
#include <iostream>

static int failures = 0;
static void expect(bool cond, const std::string& name) {
    if (!cond) { std::cerr << "FAIL: " << name << "\n"; failures++; }
}

int main() {
    // ---- resting orders with no cross produce no trades ----
    {
        OrderBook book;
        std::vector<Trade> trades;
        book.add_limit_order(Side::Buy, 9950, 100, trades);
        book.add_limit_order(Side::Sell, 10050, 100, trades);
        expect(trades.empty(), "no_cross_no_trade");
        expect(book.best_bid() == 9950, "best_bid_after_resting");
        expect(book.best_ask() == 10050, "best_ask_after_resting");
    }

    // ---- exact cross fully fills both sides ----
    {
        OrderBook book;
        std::vector<Trade> trades;
        book.add_limit_order(Side::Buy, 10000, 100, trades);
        book.add_limit_order(Side::Sell, 10000, 100, trades);
        expect(trades.size() == 1, "exact_cross_one_trade");
        expect(trades[0].quantity == 100, "exact_cross_full_quantity");
        expect(trades[0].price == 10000, "exact_cross_at_common_price");
        expect(!book.best_bid().has_value(), "book_empty_after_exact_cross_bid");
        expect(!book.best_ask().has_value(), "book_empty_after_exact_cross_ask");
    }

    // ---- price improvement: aggressor pays the resting price, not its own limit ----
    {
        OrderBook book;
        std::vector<Trade> trades;
        book.add_limit_order(Side::Sell, 10000, 100, trades); // resting ask @ $10.00
        trades.clear();
        book.add_limit_order(Side::Buy, 10100, 100, trades);  // buy willing to pay $10.10
        expect(trades.size() == 1, "price_improvement_one_trade");
        expect(trades[0].price == 10000, "price_improvement_fills_at_resting_price");
    }

    // ---- time priority: earlier order at a price level fills first ----
    {
        OrderBook book;
        std::vector<Trade> trades;
        uint64_t first = book.add_limit_order(Side::Buy, 10000, 50, trades);
        uint64_t second = book.add_limit_order(Side::Buy, 10000, 50, trades);
        trades.clear();
        book.add_limit_order(Side::Sell, 10000, 60, trades); // fills 50 from `first`, 10 from `second`
        expect(trades.size() == 2, "time_priority_two_fills");
        expect(trades[0].buy_order_id == first, "time_priority_first_order_fills_first");
        expect(trades[0].quantity == 50, "time_priority_first_fully_filled");
        expect(trades[1].buy_order_id == second, "time_priority_second_fills_remainder");
        expect(trades[1].quantity == 10, "time_priority_second_partial");
        expect(book.quantity_at(Side::Buy, 10000) == 40, "time_priority_remaining_quantity");
    }

    // ---- partial fill: incoming order larger than resting liquidity ----
    {
        OrderBook book;
        std::vector<Trade> trades;
        book.add_limit_order(Side::Sell, 10000, 30, trades);
        trades.clear();
        uint64_t buy_id = book.add_limit_order(Side::Buy, 10000, 100, trades);
        expect(trades.size() == 1 && trades[0].quantity == 30, "partial_fill_takes_all_liquidity");
        expect(book.quantity_at(Side::Buy, 10000) == 70, "partial_fill_remainder_rests");
        (void)buy_id;
    }

    // ---- cancel removes a resting order and doesn't affect others ----
    {
        OrderBook book;
        std::vector<Trade> trades;
        uint64_t a = book.add_limit_order(Side::Buy, 10000, 50, trades);
        book.add_limit_order(Side::Buy, 10000, 30, trades);
        expect(book.cancel_order(a), "cancel_existing_order_succeeds");
        expect(book.quantity_at(Side::Buy, 10000) == 30, "cancel_removes_only_that_order");
        expect(!book.cancel_order(a), "cancel_twice_fails");
        expect(!book.cancel_order(999999), "cancel_nonexistent_fails");
    }

    // ---- empty price levels are cleaned up, not left as ghosts ----
    {
        OrderBook book;
        std::vector<Trade> trades;
        uint64_t a = book.add_limit_order(Side::Buy, 10000, 50, trades);
        book.cancel_order(a);
        expect(!book.best_bid().has_value(), "level_removed_after_last_cancel");
    }

    // ---- market order takes liquidity, discards unfilled remainder ----
    {
        OrderBook book;
        std::vector<Trade> trades;
        book.add_limit_order(Side::Sell, 10000, 20, trades);
        trades.clear();
        book.add_market_order(Side::Buy, 50, trades); // only 20 available
        expect(trades.size() == 1 && trades[0].quantity == 20, "market_order_fills_available");
        expect(!book.best_ask().has_value(), "market_order_drains_book");
        expect(book.order_count() == 0, "market_order_remainder_not_resting");
    }

    if (failures == 0) std::cout << "all tests passed\n";
    return failures == 0 ? 0 : 1;
}
