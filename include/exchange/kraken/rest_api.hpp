// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/kraken/rest_api.hpp
// Kraken Spot REST API – request builders and response parsers.
// All member/function bodies are defined in src/kraken/rest_api.cpp.
//
// Namespace: exchange::kraken::rest

#include "exchange/kraken/auth.hpp"
#include "exchange/kraken/types.hpp"
#include "exchange/common/rest.hpp"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace exchange::kraken::rest {

using json = nlohmann::json;

// Re-export common scaffold types so callers only need this header.
using exchange::rest::HttpRequest;

// ── Request base classes ──────────────────────────────────────────────────────

// A Kraken public request (no auth needed).
struct PublicRequest {
    virtual ~PublicRequest() = default;
    virtual HttpRequest build() const = 0;
};

// A Kraken private request (signing required).
struct PrivateRequest {
    virtual ~PrivateRequest() = default;
    virtual HttpRequest build(const Credentials& creds) const = 0;

protected:
    // Defined in src/kraken/rest_api.cpp.
    HttpRequest make_private_request(const std::string& uri_path,
                                     std::map<std::string, std::string> params,
                                     const Credentials& creds) const;
};

// Typed bases — link each request type to its response type at compile time.
template<typename R>
struct TypedPublicRequest : PublicRequest {
    using response_type = R;
};

template<typename R>
struct TypedPrivateRequest : PrivateRequest {
    using response_type = R;
};

// ── Forward declarations ──────────────────────────────────────────────────────

struct ServerTime;
struct SystemStatus;
struct AssetInfoResult;
struct AssetPairsResult;
struct TickerResult;
struct OHLCResult;
struct OrderBookResult;
struct RecentTradesResult;
struct AccountBalanceResult;
struct ExtendedBalanceResult;
struct TradeBalance;
struct OpenOrdersResult;
struct ClosedOrdersResult;
struct QueryOrdersResultWrapper;
struct TradesHistoryResult;
struct QueryTradesResultWrapper;
struct OpenPositionsResult;
struct LedgersResult;
struct QueryLedgersResultWrapper;
struct AddOrderResult;
struct AddOrderBatchResult;
struct EditOrderResult;
struct AmendOrderResult;
struct CancelOrderResult;
struct CancelAllResult;
struct CancelAllAfterResult;
struct CancelOrderBatchResult;
struct WebSocketsTokenResult;
struct DepositMethodsResult;
struct DepositAddressesResult;
struct WithdrawResult;
struct CancelWithdrawalResult;
struct CreateSubaccountResult;
struct EarnBoolResult;

// ── MARKET DATA (public) ──────────────────────────────────────────────────────

struct GetServerTimeRequest : TypedPublicRequest<ServerTime> {
    HttpRequest build() const override;
};

struct ServerTime {
    int64_t     unixtime{0};
    std::string rfc1123;
    static ServerTime from_json(const json& j);
};

struct GetSystemStatusRequest : TypedPublicRequest<SystemStatus> {
    HttpRequest build() const override;
};

struct SystemStatus {
    std::string status;
    std::string timestamp;
    static SystemStatus from_json(const json& j);
};

struct GetAssetInfoRequest : TypedPublicRequest<AssetInfoResult> {
    std::optional<std::vector<std::string>> assets;
    std::optional<std::string>              aclass;

    HttpRequest build() const override;
};

struct AssetInfo {
    std::string aclass;
    std::string altname;
    int         decimals{0};
    int         display_decimals{0};
    static AssetInfo from_json(const json& j);
};

struct AssetInfoResult {
    std::map<std::string, AssetInfo> assets;
    static AssetInfoResult from_json(const json& j);
};

struct GetAssetPairsRequest : TypedPublicRequest<AssetPairsResult> {
    std::optional<std::vector<std::string>> pairs;
    std::optional<std::string>              info;

    HttpRequest build() const override;
};

struct AssetPairInfo {
    std::string altname;
    std::string wsname;
    std::string base;
    std::string quote;
    int         pair_decimals{0};
    int         lot_decimals{0};
    double      ordermin{0.0};
    double      costmin{0.0};
    std::vector<std::vector<double>> fees;
    std::vector<std::vector<double>> fees_maker;

    static AssetPairInfo from_json(const json& j);
};

struct AssetPairsResult {
    std::map<std::string, AssetPairInfo> pairs;
    static AssetPairsResult from_json(const json& j);
};

struct GetTickerRequest : TypedPublicRequest<TickerResult> {
    std::optional<std::vector<std::string>> pairs;

    HttpRequest build() const override;
};

struct TickerInfo {
    double ask{0.0};
    double bid{0.0};
    double last{0.0};
    double volume_today{0.0};
    double volume_24h{0.0};
    double vwap_today{0.0};
    double vwap_24h{0.0};
    int64_t trades_today{0};
    int64_t trades_24h{0};
    double  low_today{0.0};
    double  low_24h{0.0};
    double  high_today{0.0};
    double  high_24h{0.0};
    double  open{0.0};

    static TickerInfo from_json(const json& j);
};

struct TickerResult {
    std::map<std::string, TickerInfo> tickers;
    static TickerResult from_json(const json& j);
};

struct GetOHLCRequest : TypedPublicRequest<OHLCResult> {
    std::string pair;
    std::optional<int32_t>  interval;
    std::optional<int64_t>  since;

    HttpRequest build() const override;
};

struct OHLCCandle {
    int64_t time{0};
    double  open{0.0};
    double  high{0.0};
    double  low{0.0};
    double  close{0.0};
    double  vwap{0.0};
    double  volume{0.0};
    int64_t count{0};
};

struct OHLCResult {
    std::string              pair;
    std::vector<OHLCCandle>  candles;
    int64_t                  last{0};

    static OHLCResult from_json(const json& j);
};

struct GetOrderBookRequest : TypedPublicRequest<OrderBookResult> {
    std::string pair;
    std::optional<int32_t> count;

    HttpRequest build() const override;
};

struct RestBookEntry { double price{0.0}; double volume{0.0}; int64_t timestamp{0}; };

struct OrderBookResult {
    std::string                 pair;
    std::vector<RestBookEntry>  asks;
    std::vector<RestBookEntry>  bids;

    static OrderBookResult from_json(const json& j);
};

struct GetRecentTradesRequest : TypedPublicRequest<RecentTradesResult> {
    std::string pair;
    std::optional<int64_t>  since;
    std::optional<int32_t>  count;

    HttpRequest build() const override;
};

struct PublicTrade {
    double      price{0.0};
    double      volume{0.0};
    double      time{0.0};
    Side        side{Side::Buy};
    std::string order_type;
    std::string misc;
};

struct RecentTradesResult {
    std::string              pair;
    std::vector<PublicTrade> trades;
    std::string              last;

    static RecentTradesResult from_json(const json& j);
};

// ── ACCOUNT DATA (private) ────────────────────────────────────────────────────

struct GetAccountBalanceRequest : TypedPrivateRequest<AccountBalanceResult> {
    HttpRequest build(const Credentials& creds) const override;
};

struct AccountBalanceResult {
    std::map<std::string, double> balances;
    static AccountBalanceResult from_json(const json& j);
};

struct GetExtendedBalanceRequest : TypedPrivateRequest<ExtendedBalanceResult> {
    HttpRequest build(const Credentials& creds) const override;
};

struct ExtendedBalance {
    double balance{0.0};
    double hold_trade{0.0};
    double credit{0.0};
    double credit_used{0.0};
};

struct ExtendedBalanceResult {
    std::map<std::string, ExtendedBalance> balances;
    static ExtendedBalanceResult from_json(const json& j);
};

struct GetTradeBalanceRequest : TypedPrivateRequest<TradeBalance> {
    std::optional<std::string> asset;

    HttpRequest build(const Credentials& creds) const override;
};

struct TradeBalance {
    double eb{0.0};
    double tb{0.0};
    double m{0.0};
    double n{0.0};
    double c{0.0};
    double v{0.0};
    double e{0.0};
    double mf{0.0};
    std::optional<double> ml;

    static TradeBalance from_json(const json& j);
};

struct GetOpenOrdersRequest : TypedPrivateRequest<OpenOrdersResult> {
    std::optional<bool>    trades;
    std::optional<int64_t> userref;

    HttpRequest build(const Credentials& creds) const override;
};

struct OpenOrdersResult {
    std::map<std::string, exchange::kraken::OrderInfo> open;
    static OpenOrdersResult from_json(const json& j);
};

struct GetClosedOrdersRequest : TypedPrivateRequest<ClosedOrdersResult> {
    std::optional<bool>    trades;
    std::optional<int64_t> userref;
    std::optional<double>  start;
    std::optional<double>  end;
    std::optional<int32_t> ofs;
    std::optional<std::string> closetime;

    HttpRequest build(const Credentials& creds) const override;
};

struct ClosedOrdersResult {
    std::map<std::string, exchange::kraken::OrderInfo> closed;
    int32_t count{0};
    static ClosedOrdersResult from_json(const json& j);
};

struct QueryOrdersRequest : TypedPrivateRequest<QueryOrdersResultWrapper> {
    std::vector<std::string> txids;
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override;
};

struct QueryOrdersResultWrapper {
    std::map<std::string, exchange::kraken::OrderInfo> orders;
    static QueryOrdersResultWrapper from_json(const json& j);
};

struct GetTradesHistoryRequest : TypedPrivateRequest<TradesHistoryResult> {
    std::optional<std::string> type;
    std::optional<bool>        trades;
    std::optional<double>      start;
    std::optional<double>      end;
    std::optional<int32_t>     ofs;

    HttpRequest build(const Credentials& creds) const override;
};

struct TradesHistoryResult {
    std::map<std::string, exchange::kraken::TradeInfo> trades;
    int32_t count{0};
    static TradesHistoryResult from_json(const json& j);
};

struct QueryTradesRequest : TypedPrivateRequest<QueryTradesResultWrapper> {
    std::vector<std::string> txids;
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override;
};

struct QueryTradesResultWrapper {
    std::map<std::string, exchange::kraken::TradeInfo> trades;
    static QueryTradesResultWrapper from_json(const json& j);
};

struct GetOpenPositionsRequest : TypedPrivateRequest<OpenPositionsResult> {
    std::optional<std::vector<std::string>> txids;
    std::optional<bool> docalcs;
    std::optional<bool> consolidation;

    HttpRequest build(const Credentials& creds) const override;
};

struct PositionInfo {
    std::string ordertxid;
    std::string pair;
    double      time{0.0};
    Side        type{Side::Buy};
    OrderType   ordertype{OrderType::Market};
    double      cost{0.0};
    double      fee{0.0};
    double      vol{0.0};
    double      vol_closed{0.0};
    double      margin{0.0};
    double      value{0.0};
    double      net{0.0};
    std::string terms;
    std::string rollovertm;
    std::string misc;
    std::string oflags;

    static PositionInfo from_json(const json& j);
};

struct OpenPositionsResult {
    std::map<std::string, PositionInfo> positions;
    static OpenPositionsResult from_json(const json& j);
};

struct GetLedgersRequest : TypedPrivateRequest<LedgersResult> {
    std::optional<std::vector<std::string>> assets;
    std::optional<std::string> aclass;
    std::optional<std::string> type;
    std::optional<double>  start;
    std::optional<double>  end;
    std::optional<int32_t> ofs;

    HttpRequest build(const Credentials& creds) const override;
};

struct LedgersResult {
    std::map<std::string, exchange::kraken::LedgerEntry> ledger;
    int32_t count{0};
    static LedgersResult from_json(const json& j);
};

struct QueryLedgersRequest : TypedPrivateRequest<QueryLedgersResultWrapper> {
    std::vector<std::string> ids;
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override;
};

struct QueryLedgersResultWrapper {
    std::map<std::string, exchange::kraken::LedgerEntry> ledger;
    static QueryLedgersResultWrapper from_json(const json& j);
};

// ── TRADING (private) ─────────────────────────────────────────────────────────

// Helper: serialize an OrderParams into the REST param map.
// Defined in src/kraken/rest_api.cpp.
void apply_order_params_to_rest(std::map<std::string, std::string>& p,
                                const exchange::kraken::OrderParams& op);

struct AddOrderRequest : TypedPrivateRequest<AddOrderResult> {
    exchange::kraken::OrderParams params;

    HttpRequest build(const Credentials& creds) const override;
};

struct AddOrderResult {
    std::string              descr_order;
    std::optional<std::string> descr_close;
    std::vector<std::string> txids;

    static AddOrderResult from_json(const json& j);
};

struct AddOrderBatchRequest : TypedPrivateRequest<AddOrderBatchResult> {
    std::string pair;
    std::vector<exchange::kraken::OrderParams> orders;
    std::optional<bool>   validate;
    std::optional<std::string> deadline;

    HttpRequest build(const Credentials& creds) const override;
};

struct BatchOrderResult {
    std::string              descr_order;
    std::vector<std::string> txids;
    std::optional<std::string> error;
};

struct AddOrderBatchResult {
    std::vector<BatchOrderResult> orders;
    static AddOrderBatchResult from_json(const json& j);
};

struct EditOrderRequest : TypedPrivateRequest<EditOrderResult> {
    std::string txid;
    std::string pair;
    std::optional<double>      volume;
    std::optional<double>      price;
    std::optional<double>      price2;
    std::optional<double>      display_vol;
    std::optional<bool>        post_only;
    std::optional<std::string> deadline;
    std::optional<int64_t>     userref;
    std::optional<std::string> cl_ord_id;
    std::optional<bool>        validate;

    HttpRequest build(const Credentials& creds) const override;
};

struct EditOrderResult {
    std::string              descr_order;
    std::vector<std::string> txids;
    std::optional<std::string> orig_txid;

    static EditOrderResult from_json(const json& j);
};

struct AmendOrderRequest : TypedPrivateRequest<AmendOrderResult> {
    std::optional<std::string> txid;
    std::optional<std::string> cl_ord_id;
    std::optional<double>      order_qty;
    std::optional<double>      display_qty;
    std::optional<double>      limit_price;
    std::optional<double>      trigger_price;
    std::optional<std::string> deadline;

    HttpRequest build(const Credentials& creds) const override;
};

struct AmendOrderResult {
    std::string amend_id;
    static AmendOrderResult from_json(const json& j);
};

struct CancelOrderRequest : TypedPrivateRequest<CancelOrderResult> {
    std::string txid;

    HttpRequest build(const Credentials& creds) const override;
};

struct CancelOrderResult {
    int32_t count{0};
    bool    pending{false};
    static CancelOrderResult from_json(const json& j);
};

struct CancelAllOrdersRequest : TypedPrivateRequest<CancelAllResult> {
    HttpRequest build(const Credentials& creds) const override;
};

struct CancelAllResult {
    int32_t count{0};
    static CancelAllResult from_json(const json& j);
};

struct CancelAllOrdersAfterRequest : TypedPrivateRequest<CancelAllAfterResult> {
    int32_t timeout{0};

    HttpRequest build(const Credentials& creds) const override;
};

struct CancelAllAfterResult {
    std::string current_time;
    std::string trigger_time;
    static CancelAllAfterResult from_json(const json& j);
};

struct CancelOrderBatchRequest : TypedPrivateRequest<CancelOrderBatchResult> {
    std::vector<std::string> orders;

    HttpRequest build(const Credentials& creds) const override;
};

struct CancelOrderBatchResult {
    int32_t count{0};
    static CancelOrderBatchResult from_json(const json& j);
};

struct GetWebSocketsTokenRequest : TypedPrivateRequest<WebSocketsTokenResult> {
    HttpRequest build(const Credentials& creds) const override;
};

struct WebSocketsTokenResult {
    std::string token;
    int64_t     expires{0};
    static WebSocketsTokenResult from_json(const json& j);
};

// ── FUNDING (private) ─────────────────────────────────────────────────────────

struct GetDepositMethodsRequest : TypedPrivateRequest<DepositMethodsResult> {
    std::string asset;
    HttpRequest build(const Credentials& creds) const override;
};

struct DepositMethod {
    std::string method;
    std::string limit;
    std::string fee;
    bool        gen_address{false};
    static DepositMethod from_json(const json& j);
};

struct DepositMethodsResult {
    std::vector<DepositMethod> methods;
    static DepositMethodsResult from_json(const json& j);
};

struct GetDepositAddressesRequest : TypedPrivateRequest<DepositAddressesResult> {
    std::string asset;
    std::string method;
    std::optional<bool> new_address;
    HttpRequest build(const Credentials& creds) const override;
};

struct DepositAddress {
    std::string address;
    std::string expiretm;
    bool        new_addr{false};
    static DepositAddress from_json(const json& j);
};

struct DepositAddressesResult {
    std::vector<DepositAddress> addresses;
    static DepositAddressesResult from_json(const json& j);
};

struct WithdrawRequest : TypedPrivateRequest<WithdrawResult> {
    std::string asset;
    std::string key;
    std::string amount;
    std::optional<std::string> address;
    std::optional<std::string> max_fee;

    HttpRequest build(const Credentials& creds) const override;
};

struct WithdrawResult {
    std::string refid;
    static WithdrawResult from_json(const json& j);
};

struct CancelWithdrawalRequest : TypedPrivateRequest<CancelWithdrawalResult> {
    std::string asset;
    std::string refid;
    HttpRequest build(const Credentials& creds) const override;
};

struct CancelWithdrawalResult {
    bool result{false};
    static CancelWithdrawalResult from_json(const json& j);
};

// ── SUBACCOUNTS (private) ─────────────────────────────────────────────────────

struct CreateSubaccountRequest : TypedPrivateRequest<CreateSubaccountResult> {
    std::string username;
    std::string email;
    HttpRequest build(const Credentials& creds) const override;
};

struct CreateSubaccountResult {
    bool result{false};
    static CreateSubaccountResult from_json(const json& j);
};

// ── EARN (private) ────────────────────────────────────────────────────────────

struct AllocateEarnRequest : TypedPrivateRequest<EarnBoolResult> {
    std::string strategy_id;
    std::string amount;
    HttpRequest build(const Credentials& creds) const override;
};

struct DeallocateEarnRequest : TypedPrivateRequest<EarnBoolResult> {
    std::string strategy_id;
    std::string amount;
    HttpRequest build(const Credentials& creds) const override;
};

struct EarnBoolResult {
    bool result{false};
    static EarnBoolResult from_json(const json& j);
};

} // namespace exchange::kraken::rest
