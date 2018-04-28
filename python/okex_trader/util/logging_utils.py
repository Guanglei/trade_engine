import os, gzip, time

import logging
from logging.handlers import RotatingFileHandler

class EnhancedRotatingFileHandler(RotatingFileHandler):
    def __init__(self, filename, mode='a', maxBytes=0, backupCount=0, encoding=None, delay=False, maxRotateInterval=0):
        self.max_rotate_interval_ = maxRotateInterval
        self.last_ts_ = int(time.time())

        RotatingFileHandler.__init__(self, filename, mode, maxBytes, backupCount, encoding, delay)
    
    def doRollover(self):
        self.last_ts_ = int(time.time())
        return RotatingFileHandler.doRollover(self)

    def shouldRollover(self, record):
        ret = False
        if self.max_rotate_interval_ > 0:
            ret = (int(time.time()) - self.last_ts_) > self.max_rotate_interval_
        return ret or RotatingFileHandler.shouldRollover(self, record)
 
class gzip_rotator:
    def __init__(self, base_dir, exchange, channel):
        self.base_dir_ = base_dir
        self.exchange_ = exchange
        self.channel_ = channel

        if not os.path.exists(base_dir):
            os.makedirs(base_dir)

    def __call__(self, source, dest):
        dir_name = time.strftime("%Y%m%d")
        date_dir_path = os.path.join(self.base_dir_, dir_name)

        if not os.path.exists(date_dir_path):
            os.makedirs(date_dir_path)
        
        exchange_dir_path = os.path.join(date_dir_path, self.exchange_)
        if not os.path.exists(exchange_dir_path):
            os.makedirs(exchange_dir_path)

        channel_dir_path = os.path.join(exchange_dir_path, self.channel_)
        if not os.path.exists(channel_dir_path):
            os.makedirs(channel_dir_path)

        new_dest = time.strftime("%Y%m%d%H%M%S")
        new_dest = os.path.join(channel_dir_path, new_dest)
	
        os.rename(source, new_dest)	
        f_in = open(new_dest, 'rb')
        f_out = gzip.open("{}.gz".format(new_dest), 'wb')
        f_out.writelines(f_in)
        f_out.close()
        f_in.close()
        os.remove(new_dest)

def create_logger(base_dir, exchange, channel, rotate_size, rotate_interval):
    temp_data_file = "/tmp/{}_{}.tmp".format(exchange, channel)
    if os.path.exists(temp_data_file):
        os.remove(temp_data_file)

    log_handler = EnhancedRotatingFileHandler(temp_data_file, mode='a', maxBytes=rotate_size, \
	                            backupCount=10000, encoding=None, delay=0, maxRotateInterval=rotate_interval)
    log_handler.setLevel(logging.INFO)
    log_handler.rotator = gzip_rotator(base_dir, exchange, channel)

    logger = logging.getLogger('{}_{}'.format(exchange, channel))
    logger.setLevel(logging.INFO)
    logger.addHandler(log_handler)
    
    return logger
 
