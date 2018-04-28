import websocket
import hashlib
import zlib
import sys, os
import json
import itertools
import urllib.request
import time

import logging

from util.logging_utils import gzip_rotator, EnhancedRotatingFileHandler, create_logger

import pdb

url = "wss://real.okex.com:10440/websocket/okexapi"

apikey = 'a815131d-b1f4-4c76-91f9-e016b46dce60'
secretkey = 'A77B2084AFC76AF0A74EF16AF3FD2BCD'

exchange = "okex"

channel_type_map = {}

quote_bid = None
quote_ask = None

passive_quote_offset = 10
passive_quote_retreat_buffer = 5
passive_quote_advance_buffer = 2

def on_message(self, evt):
    if isinstance(evt, bytes):
        data = inflate(evt)  # data decompress
        data = data.decode('utf-8')
        jdata = json.loads(data)
    else:
        jdata = json.loads(evt)
    
    #print(jdata)
    if 'channel' in jdata[0]: 
        channel = jdata[0]['channel']
        """
        if channel == 'ok_sub_spot_btc_usdt_order':
            payload = jdata[0]['data']
            
            print(payload)

            order_id = payload['orderId']    
            status = payload['status']

            if status == 0:
                time.sleep(5)
    
                order_cancel = spotCancelOrder(channel = 'ok_spot_cancel_order', api_key = apikey, 
                                secretkey = secretkey, symbol = 'btc_usdt',orderId = str(order_id))
                print(order_cancel)
                self.send(order_cancel)
        """ 
        if channel == 'ok_sub_spot_btc_usdt_depth_5':
            payload = jdata[0]['data']
            #print(payload)

            bids = payload['bids']
            if len(bids) > 0:
                bid_px = float(bids[0][0])
                bid_qty = float(bids[0][1])
                #print("bid {} @ {}".format(bid_qty, bid_px))    
                
                quote_changed = False
                global quote_bid
                if not quote_bid:
                    quote_bid = bid_px - passive_quote_offset
                    quote_changed = True
                elif quote_bid > (bid_px - passive_quote_retreat_buffer):
                    quote_bid = bid_px - passive_quote_offset
                    quote_changed = True
                elif quote_bid <= (bid_px - passive_quote_offset - passive_quote_advance_buffer):
                    quote_bid = bid_px - passive_quote_offset
                    quote_changed = True

                if quote_changed:
                    print("current bid quote {}".format(quote_bid))

            asks = payload['asks']
            if len(asks) > 0:
                ask_px = float(asks[-1][0])
                ask_qty = float(asks[-1][1])
                #print("ask {} @ {}".format(ask_qty, ask_px))    
                
                quote_changed = False

                global quote_ask
                if not quote_ask:
                    quote_ask = ask_px + passive_quote_offset
                    quote_changed = True
                elif quote_ask < (ask_px + passive_quote_retreat_buffer):
                    quote_ask = ask_px + passive_quote_offset
                    quote_changed = True
                elif quote_ask >= (ask_px + passive_quote_offset + passive_quote_advance_buffer):
                    quote_ask = ask_px + passive_quote_offset
                    quote_changed = True

                if quote_changed:
                    print("current ask quote {}".format(quote_ask))



def getLogin(api_key, secretkey):
    params = {
        'api_key': api_key,
    }
    sign = buildMySign(params, secretkey)
    return "{'event':'login','parameters':{'api_key':'" + api_key + "','sign':'" + sign + "'}}"

def getOkSpotUserinfo(api_key, secretkey):
    params = {
        'api_key': api_key,
    }
    sign = buildMySign(params, secretkey)
    return "{'event':'addChannel','channel':'ok_spot_userinfo','parameters':{'api_key':'" + api_key + "','sign':'" + sign + "'}}"

def on_open(self):
    
    #add spot channels
    spot_coins = ("btc_usdt",)

    """
    spot_channel_tmp = {"ok_sub_spot_{}_ticker" : "spot_ticker", "ok_sub_spot_{}_depth" : "spot_depth", \
                        "ok_sub_spot_{}_depth_5" : "spot_depth_5", "ok_sub_spot_{}_depth_10" : "spot_depth_10", \
                        "ok_sub_spot_{}_depth_20" : "spot_depth_20", "ok_sub_spot_{}_deals" : "spot_trade"}
    """

    spot_channel_tmp = {"ok_sub_spot_{}_depth_5" : "spot_depth_5", "ok_sub_spot_{}_deals" : "spot_trade"}
    for ct in spot_channel_tmp.keys():
        for c in spot_coins:
            ch = ct.format(c)
            channel_type_map[ch] = spot_channel_tmp[ct]
            self.send("{'event':'addChannel','channel':'" + ch + "', 'binary':'1'}")


    """
    sendLogin = getLogin(apikey, secretkey)
    print(sendLogin)
    self.send(sendLogin)

    sendOkSpotUserinfo = getOkSpotUserinfo(apikey, secretkey)
    print(sendOkSpotUserinfo)
    self.send(sendOkSpotUserinfo)

    price = '8700'
    amount = '0.001'

    order_buy = spotTrade(channel = 'ok_spot_order', api_key = apikey, secretkey = secretkey,
        symbol = 'btc_usdt',tradeType = 'buy',price = price, amount = amount)
    print(order_buy)
    self.send(order_buy)
    """    
    


    # subscrib real trades for self
    # realtradesMsg = realtrades('ok_sub_spotusd_trades',api_key,secret_key)
    # self.send(realtradesMsg)

    # spot trade via websocket
    # spotTradeMsg = spotTrade('ok_spotusd_trade',api_key,secret_key,'ltc_usd','buy_market','1','')
    # self.send(spotTradeMsg)

    # spot trade cancel
    # spotCancelOrderMsg = spotCancelOrder('ok_spotusd_cancel_order',api_key,secret_key,'btc_usd','125433027')
    # self.send(spotCancelOrderMsg)

    # future trade
    # futureTradeMsg = futureTrade(api_key,secret_key,'btc_usd','this_week','','2','1','1','20')
    # self.send(futureTradeMsg)

    # future trade cancel
    # futureCancelOrderMsg = futureCancelOrder(api_key,secret_key,'btc_usd','65464','this_week')
    # self.send(futureCancelOrderMsg)

    # subscrbe future trades for self
    # futureRealTradesMsg = futureRealTrades(api_key,secret_key)
    # self.send(futureRealTradesMsg)


def inflate(data):
    decompress = zlib.decompressobj(
        -zlib.MAX_WBITS  # see above
    )
    inflated = decompress.decompress(data)
    inflated += decompress.flush()
    return inflated


def on_error(self, evt):
    print('error:')
    print (evt)


def on_close(self, code, reason):
    print ('disconnect {} {}'.format(code, reason))
    #on_connecion()


def on_reconnect(self):
    print('reconnect')
    on_connecion()


def on_connecion():
    ws = websocket.WebSocketApp(url,
                                on_message=on_message,
                                on_error=on_error,
                                on_close=on_close)
    ws.on_open = on_open
    ws.on_reconnect = on_reconnect
    ws.run_forever(ping_interval=5)


# business
def buildMySign(params, secretKey):
    sign = ''
    for key in sorted(params.keys()):
        sign += key + '=' + str(params[key]) + '&'
    return hashlib.md5((sign + 'secret_key=' + secretKey).encode("utf-8")).hexdigest().upper()


# spot trade
def spotTrade(channel, api_key, secretkey, symbol, tradeType, price='', amount=''):
    params = {
        'api_key': api_key,
        'symbol': symbol,
        'type': tradeType
    }
    if price:
        params['price'] = price
    if amount:
        params['amount'] = amount
    sign = buildMySign(params, secretkey)
    finalStr = "{'event':'addChannel','channel':'" + channel + "','parameters':{'api_key':'" + api_key + "','sign':'" + sign + "','symbol':'" + symbol + "','type':'" + tradeType + "'"
    if price:
        finalStr += ",'price':'" + price + "'"
    if amount:
        finalStr += ",'amount':'" + amount + "'"
    finalStr += "}}"
    return finalStr


# spot cancel order
def spotCancelOrder(channel, api_key, secretkey, symbol, orderId):
    params = {
        'api_key': api_key,
        'symbol': symbol,
        'order_id': orderId
    }
    sign = buildMySign(params, secretkey)
    return "{'event':'addChannel','channel':'" + channel + "','parameters':{'api_key':'" + api_key + "','sign':'" + sign + "','symbol':'" + symbol + "','order_id':'" + orderId + "'},'binary':'1'}"


# subscribe trades for self
def realtrades(channel, api_key, secretkey):
    params = {'api_key': api_key}
    sign = buildMySign(params, secretkey)
    return "{'event':'addChannel','channel':'" + channel + "','parameters':{'api_key':'" + api_key + "','sign':'" + sign + "'},'binary':'true'}"


# trade for future
def futureTrade(api_key, secretkey, symbol, contractType, price='', amount='', tradeType='', matchPrice='',
                leverRate=''):
    params = {
        'api_key': api_key,
        'symbol': symbol,
        'contract_type': contractType,
        'amount': amount,
        'type': tradeType,
        'match_price': matchPrice,
        'lever_rate': leverRate
    }
    if price:
        params['price'] = price
    sign = buildMySign(params, secretkey)
    finalStr = "{'event':'addChannel','channel':'ok_futuresusd_trade','parameters':{'api_key':'" + api_key + "',\
               'sign':'" + sign + "','symbol':'" + symbol + "','contract_type':'" + contractType + "'"
    if price:
        finalStr += ",'price':'" + price + "'"
    finalStr += ",'amount':'" + amount + "','type':'" + tradeType + "','match_price':'" + matchPrice + "','lever_rate':'" + leverRate + "'},'binary':'true'}"
    return finalStr


# future trade cancel
def futureCancelOrder(api_key, secretkey, symbol, orderId, contractType):
    params = {
        'api_key': api_key,
        'symbol': symbol,
        'order_id': orderId,
        'contract_type': contractType
    }
    sign = buildMySign(params, secretkey)
    return "{'event':'addChannel','channel':'ok_futuresusd_cancel_order','parameters':{'api_key':'" + api_key + "',\
            'sign':'" + sign + "','symbol':'" + symbol + "','contract_type':'" + contractType + "','order_id':'" + orderId + "'},'binary':'true'}"


# subscribe future trades for self
def futureRealTrades(api_key, secretkey):
    params = {'api_key': api_key}
    sign = buildMySign(params, secretkey)
    return "{'event':'addChannel','channel':'ok_sub_futureusd_trades','parameters':{'api_key':'" + api_key + "','sign':'" + sign + "'},'binary':'true'}"


if __name__ == "__main__":
    on_connecion()
