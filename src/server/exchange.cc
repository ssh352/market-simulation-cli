#include "server/exchange.h"

#include "boost/property_tree/ptree.hpp"

#include "common/jsonxx.h"

namespace server {

Exchange::Exchange(boost::asio::io_service& io_service, bp::ptree config)
  : quotes_reader_(io_service, config, this),
    connection_manager_(),
    server_("127.0.0.1", config.get("port", 9000), io_service, &connection_manager_,
        [=] (std::string connection_name, std::string data) {}),
    quotes_subs_(&connection_manager_),
    orders_subs_(&connection_manager_),
    trades_subs_(&connection_manager_),
    ticker_(config.get("ticker", "601398")),
    bid_price_(0),
    ask_price_(0),
    bid_qty_(0),
    ask_qty_(0),
    last_(0),
    turnover_(0) {}

Exchange::~Exchange() {}

void Exchange::Start() {
  quotes_reader_.Start();
}

void Exchange::Stop() {
  quotes_reader_.Stop();
}

void Exchange::OnQuote(std::string datetime, std::string ticker, double bid_price, double ask_price, int bid_qty, int ask_qty) {
  if (ticker_ != ticker) {
    std::cout << "Exchange only interested in ticker[" << ticker_ << "], recived[" << ticker << "]" << std::endl;
    return;
  }
  bid_price_  = bid_price;
  ask_price_  = ask_price;
  bid_qty_    = bid_qty;
  ask_qty_    = ask_qty;

  jsonxx::Object obj;
  obj << "time"   << datetime;
  obj << "ticker" << ticker_;
  obj << "bid_price" << std::to_string(bid_price_);
  obj << "ask_price" << std::to_string(ask_price_);
  obj << "bid_qty"   << bid_qty_;
  obj << "ask_qty"   << ask_qty_;
  obj << "last"      << std::to_string(last_);
  obj << "turnover"  << std::to_string(turnover_);
  auto str = obj.json();
  str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());

  std::cout << "published: " << str << std::endl;
  quotes_subs_.Publish(ticker_, str);

  // get all bid orders above ask_price
  // get all ask orders below bid_price
  // fill all these orders
}

void Exchange::OnRequest(ReqMarkets  req) {
  quotes_subs_.Subscribe(req.instrested_ticker, req.login_account);
  std::cout << "account[" << req.login_account << "] registered quotes info for ticker[" << req.instrested_ticker << "]" << std::endl;
}

void Exchange::OnRequest(ReqOrders   req) {
  if (req.instrested_account.empty()) {
    std::cout << "Please specify account name" << std::endl;
    return;
  }
  if (req.instrested_account == "server") {
    orders_subs_.SubscribeAll(req.login_account);
    std::cout << "account[" << req.login_account << "] registered orders info all server accounts" << std::endl;
  } else {
    orders_subs_.Subscribe(req.instrested_account, req.login_account);
    std::cout << "account[" << req.login_account << "] registered orders info for account[" << req.instrested_account << "]" << std::endl;
  }
}

void Exchange::OnRequest(ReqTrades   req) {
  if (req.instrested_account.empty()) {
    std::cout << "Please specify account name" << std::endl;
    return;
  }
  if (req.instrested_account == "server") {
    trades_subs_.SubscribeAll(req.login_account);
    std::cout << "account[" << req.login_account << "] registered trades info all server accounts" << std::endl;
  } else {
    trades_subs_.Subscribe(req.instrested_account, req.login_account);
    std::cout << "account[" << req.login_account << "] registered trades info for account[" << req.instrested_account << "]" << std::endl;
  }
}

void Exchange::OnRequest(ReqOms      req) {
  orders_subs_.Subscribe(req.login_account, req.login_account);
  trades_subs_.Subscribe(req.login_account, req.login_account);
  std::cout << "account[" << req.login_account << "] connected to oms" << std::endl;
}

void Exchange::OnRequest(ReqNewOrder req) {
  // match with current quotes
  // matched ? ==> fill order
  // otherwise keep  it in orderbook
}

void Exchange::UpdateStats(double fill_price, int fill_qty) {
  last_     = fill_price;
  turnover_ += fill_price * fill_qty;
}

}
