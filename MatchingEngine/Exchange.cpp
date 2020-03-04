//
// Created by jccai on 19-5-24.
//

#include "Exchange.h"

using MappingUtils::Fields;

ExchangeExecutor::ExchangeExecutor(Exchange *exchange)
  : exchange_(exchange)
{

}

void ExchangeExecutor::run()
{
  while (true) {
    vector<OrderPtr> orders;
    // fixme
    exchange_->bufferedOrders_->pop(orders, 100000);
    for (auto &order : orders) {
      if (!order) {
        return;
      }
      switch(order->order_op()) {
        case book::ADD:
          exchange_->book_->add(order);
          break;
        case book::MODIFY:
          exchange_->book_->replace(order);
          break;
        case book::CANCEL:
          exchange_->book_->cancel(order->order_id());
          break;
          //default:
          //  errMsg = "unknown order operation";
      }
    }
    if (exchange_->bufferedOutputTable_->size()) {
      exchange_->outputToOutput();
    }
  }
}

Config Exchange::config_;

Exchange::Exchange(
  std::string &&symbol,
  std::string &&creator,
  TableSP outputTable)
  : AbstractStreamEngine(__func__, symbol, creator),
    symbol_(symbol),
    bufferedOrders_(new OrderQueue()),
    outputTable_(outputTable)
{
  if(!config_.initialized) {
    throw RuntimeException("global config is not initialized");
  }
  if (config_.bookDepth_) {
    auto book = std::make_shared<DepthOrderBook>(symbol_, config_.bookDepth_);
    book->set_bbo_listener(this);
    book->set_depth_listener(this);
    book_ = book;
  } else {
    book_ = std::make_shared<OrderBook>(symbol_);
  }
  book_->set_order_listener(this);
  book_->set_trade_listener(this);
  book_->set_order_book_listener(this);
  bufferedOutputTable_ = createTableFromModel(outputTable_);
  for (INDEX i = 0; i < outputTable_->columns(); ++i) {
    bufferedOutput_.push_back(
      Util::createVector(outputTable_->getColumnType(i), 1));
  }
  executorThread_ = new Thread(new ExchangeExecutor(this));
}

Exchange::~Exchange()
{
  bufferedOrders_->push(nullptr);
  executorThread_->join();
}

bool Exchange::append(
  vector<ConstantSP> &values,
  INDEX &insertedRows,
  string &errMsg)
{
  vector<ConstantSP> val;
  if(values[0]->isTable()) {
    for(INDEX i = 0; i < values[0]->columns(); ++i) {
      val.emplace_back(values[0]->getColumn(i));
    }
  } else {
    val = values;
  }
  LockGuard<Mutex> guard(&mutex_);
  if (!executorThread_->isRunning()) {
    executorThread_->start();
  }
  auto rows = val[0]->size();
  for(INDEX i = 0; i < rows; ++i) {
    auto op = static_cast<OrderOperation>(
      val[config_.mapping_[Fields::op]]->getInt(i));
    auto accountUid = static_cast<uint64_t>(
      val[config_.mapping_[Fields::accountUid]]->getLong(i));
    auto symbol = static_cast<string>(
      val[config_.mapping_[Fields::symbol]]->getString(i));
    auto id = static_cast<uint64_t>(
      val[config_.mapping_[Fields::id]]->getLong(i));
    auto quantity = static_cast<uint64_t>(
      val[config_.mapping_[Fields::quantity]]->getLong(i));
    auto condition = static_cast<uint32_t>(
      val[config_.mapping_[Fields::condition]]->getInt(i));
    auto price = static_cast<uint64_t>(
      val[config_.mapping_[Fields::price]]->getLong(i));
    auto thresholdPrice = static_cast<uint64_t>(
      val[config_.mapping_[Fields::thresholdPrice]]->getLong(i));
    auto expiredTime = static_cast<uint64_t>(
      val[config_.mapping_[Fields::expiredTime]]->getLong(i));

    auto order = std::make_shared<book::Order>(op, accountUid,
      std::move(symbol), id, quantity, condition,
      price, thresholdPrice, expiredTime);
    bufferedOrders_->push(order);
  }
  cumMessages_ += rows;
  insertedRows = rows;
  return true;
}

TableSP Exchange::generateEngineStatTable()
{
  static vector<string> colNames{
    "name", "user", "status", "lastErrMsg", "numRows", "queueDepth"
  };
  vector<ConstantSP> cols;
  cols.push_back(Util::createVector(DT_STRING, 0));
  cols.push_back(Util::createVector(DT_STRING, 0));
  cols.push_back(Util::createVector(DT_STRING, 0));
  cols.push_back(Util::createVector(DT_STRING, 0));
  cols.push_back(Util::createVector(DT_LONG, 0));
  cols.push_back(Util::createVector(DT_LONG, 0));
  return Util::createTable(colNames, cols);
}

void Exchange::initEngineStat()
{
  engineStat_.push_back(new String(engineName_));
  engineStat_.push_back(new String(engineUser_));
  engineStat_.push_back(new String(status_));
  engineStat_.push_back(new String(lastErrMsg_));
  engineStat_.push_back(new Long(cumMessages_));
  engineStat_.push_back(new Long(bufferedOrders_->size()));
}

void Exchange::updateEngineStat()
{
  LockGuard<Mutex> guard(&mutex_);
  engineStat_[2]->setString(status_);
  engineStat_[3]->setString(lastErrMsg_);
  engineStat_[4]->setLong(cumMessages_);
  engineStat_[5]->setLong(bufferedOrders_->size());
}


ConstantSP Exchange::setupGlobalConfig(Heap *heap, vector<ConstantSP> &args)
{
  static string funcName = "setupGlobalConfig";
  static string syntax =
    "Usage: " + funcName + "(inputScheme, [mapping], [pricePrecision=3], [bookDepth=10]).";

  // check inputScheme
  if (!args[0]->isTable()) {
    throw IllegalArgumentException(
      "setupGlobalConfig",
      "inputScheme must be a table");
  }
  TableSP inputScheme = args[0];

  // check mapping
  DictionarySP fieldMap;
  if (args.size() > 1) {
    if (!args[1]->isNothing() && !args[1]->isDictionary()) {
      throw IllegalArgumentException(
        "setupGlobalConfig",
        "mapping must be a dictionary");
    } else if (args[1]->isNothing()) {
      fieldMap = nullptr;
    } else {
      fieldMap = args[1];
    }
  }

  // check pricePrecision
  uint32_t pricePrecision = 3;
  if (args.size() > 2) {
    if (args[2]->isNull() || args[2]->getCategory() != INTEGRAL) {
      throw IllegalArgumentException(
        "setupGlobalConfig",
        "pricePrecision must be a non-negative integer");
    }
    pricePrecision = args[2]->getLong();
  }

  // check bookDepth
  uint32_t bookDepth = 10;
  if (args.size() > 3) {
    if (args[3]->isNull() || args[3]->getCategory() != INTEGRAL) {
      throw IllegalArgumentException(
        "setupGlobalConfig",
        "bookDepth must be a non-negative integer");
    }
    bookDepth = args[3]->getLong();
  }

  Config config;
  config.inputScheme_ = inputScheme;
  if(fieldMap.isNull()) {
    for (auto &field : MappingUtils::fieldsNames) {
      config.mapping_.emplace_back(
        inputScheme->getColumnIndex(field));
    }
  } else {
    for (auto &field : MappingUtils::fieldsNames) {
      config.mapping_.emplace_back(
        inputScheme->getColumnIndex(fieldMap->getMember(field)->getString()));
    }
  }

  config.pricePrecision_ = std::pow(10, pricePrecision);
  config.bookDepth_ = bookDepth;
  for (INDEX i = 0; i < inputScheme->columns(); ++i) {
    config.inputColumnNames_.emplace_back(inputScheme->getColumnName(i));
  }
  config.validate();
  Exchange::config_ = config;
  return new String("Successful");
}

ConstantSP Exchange::createExchange(Heap *heap, vector<ConstantSP> &args)
{
  static string funcName = "createExchange";
  static string syntax = "Usage: " + funcName + "(symbol, outputTable).";

  // check symbol
  if (args[0]->getType() != DT_STRING) {
    throw IllegalArgumentException(
      "createExchange",
      "symbol must be a string or symbol");
  }

  // check duplicate
  string symbol = args[0]->getString();
  if (!StreamEngineManager::instance().find(symbol).isNull()) {
    throw IllegalArgumentException(
      "createExchange",
      "Exchange for symbol " + symbol + " already exists");
  }

  // check outputTable
  if (!args[1]->isTable()) {
    throw IllegalArgumentException(
      "createExchange",
      "outputTable must be a table");
  }

  // check inputBufferSize
//  uint64_t inputBufferSize = 1000000;
//  if (args.size() > 2) {
//    if (args[2]->getCategory() != INTEGRAL || args[2]->getLong() < 0) {
//      throw IllegalArgumentException(
//        "createExchange",
//        "inputBufferSize must be a positive integer");
//    }
//    inputBufferSize = args[2]->getLong();
//  }

  AbstractStreamEngineSP exchange = new Exchange(
    std::move(symbol),
    heap->currentSession()->getUser()->getUserId(),
    args[1]);
  exchange->initEngineStat();
  StreamEngineManager::instance().insert(exchange);
  return exchange;
}

ConstantSP setupGlobalConfig(Heap *heap, vector<ConstantSP> &args)
{
  return Exchange::setupGlobalConfig(heap, args);
}

ConstantSP createExchange(Heap *heap, vector<ConstantSP> &args)
{
  return Exchange::createExchange(heap, args);
}
