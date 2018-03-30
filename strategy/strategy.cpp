#include "strategy.h"
#include "order_entry.h"

namespace trade_engine
{
	void strategy::deliver_md_update_event(const instrument* inst)
	{
		md_event e;
		e.internal_inst_id = inst->internal_id;
		static_cast<top_of_book&>(e) = inst->tob;
		md_event_queue_.enqueue(e);
	}

	void strategy::deliver_order_event(order_event& e)
	{
		LOGGER << "strategy::deliver_order_event: " << e.type;

		order_event_queue_.enqueue(e);
	}

	void strategy::process_gui_command()
	{
		try
        {
			char gui_msg_buf[1001] = {};
			boost::system::error_code error;
            size_t len = socket_.read_some(boost::asio::buffer(gui_msg_buf, sizeof(gui_msg_buf)), error);
			if(len > 0 && gui_msg_buf[0] == 'C')
			{
				LOGGER << "Got a command from GUI...";
				gui_command* command = reinterpret_cast<gui_command*>(&gui_msg_buf[1]);
				switch(command->CommandType)
				{
				case 0:
					{
						//should configure trading intrument in backend trade engine instead of told by GUI
						LOGGER << "GUI initialize instruments: " << command->InstrumentID[0] 
							   << " - " << command->InstrumentID[1];
						break;
					}
				case 1:
					{
						LOGGER << "GUI " << (command->CombOffsetFlag == '0' ? "open" : "close") << " for " 
							<< command->VolumeTotalOriginal << " with price margin of " << command->PriceMargin 
							<< " for instrument pair " << command->InstrumentID[0] << " - " << command->InstrumentID[1];

						if((strcmp(near_inst_->id, command->InstrumentID[0]) != 0) || (strcmp(far_inst_->id, command->InstrumentID[1]) != 0))
						{
							LOGGER << "Invalid instrument id requested sent from GUI, ignore the request...";
							return;
						}

						if(command->CombOffsetFlag == '0')
						{
							if(current_active_combo_)
							{
								LOGGER << "There is already a active combo, can't open again...";
								//TODO need to return this to GUI
							}

							//check current px diff is not less than the price margin sent from GUI
							//TODO price margin need to be identified as buy or sell the spread
							//if it is a buy price margin check if the current price diff less than the margin
							//if it is a sell price margin check if the current price diff greater than the margin
							double cur_px_diff = near_inst_->tob.last_price - far_inst_->tob.last_price;
							if(cur_px_diff > command->PriceMargin)
							{
								//sell near inst and buy far inst. Buy far inst first since it is less liquidity than near inst
								int far_order_ref = 0;
								if(oe_.send_order_insert_request(far_inst_->id, command->CombOffsetFlag, 
									THOST_FTDC_D_Buy, command->VolumeTotalOriginal, far_order_ref))
								{
									strategy_priv* priv = new strategy_priv;
									
									priv->far_pos.direction = position_direction::buy;
									priv->far_pos.inst = far_inst_;
									priv->far_pos.status = position_status::pending_open;
									priv->far_pos.quantity = command->VolumeTotalOriginal;
									priv->far_pos.open_order_ref = far_order_ref;
								
									priv->near_pos.direction = position_direction::sell;
									priv->near_pos.inst = near_inst_;
									priv->near_pos.quantity = command->VolumeTotalOriginal;
									
									priv->price_margin = command->PriceMargin;
									priv->status = position_status::pending_open;

									orderref_2_priv_map_[far_order_ref] = priv;
									current_active_combo_ = priv;
								}
								else
								{
									//TODO not event successfully send the first order out, fail the whole command
									//should let GUI know it is failed
								}
							}
						}
						else if(command->CombOffsetFlag == '1')
						{
							if(!current_active_combo_ || 
									current_active_combo_->status == position_status::pending_close 
											|| current_active_combo_->status == position_status::close)
							{
								LOGGER << "The combo is already during close, ignore the close command...";
							}
							else
							{
								current_active_combo_->status = position_status::pending_close;
								if(current_active_combo_->near_pos.status == position_status::open)
								{
									close_position(current_active_combo_->near_pos, current_active_combo_->near_pos.open_trade_quantity);
								}
								if(current_active_combo_->far_pos.status == position_status::open)
								{
									close_position(current_active_combo_->far_pos, current_active_combo_->near_pos.open_trade_quantity);
								}
							}
						}
						break;
					}
				case 2:
					{
						//GUI did not implement this part, need to figure out
						LOGGER << "GUI cancel order...";
						break;
					}
				default:
					{
						LOGGER << "unknow GUI command type: " << command->CommandType;
						break;
					}
				}
			}
        }
        catch(std::exception& e)
        {
			LOGGER << "Got exception when reading from GUI socket: " << e.what();
        }
	}

	void strategy::handle_order_event(order_event& e)
	{
		switch(e.type)
		{
			case order_event_type::accepted:
			{
				LOGGER << "order insert acceptted";
				boost::unordered_map<int, strategy_priv*>::iterator i = orderref_2_priv_map_.find(e.order_ref);
				if(i == orderref_2_priv_map_.end())
				{
					LOGGER << "Failed to find strategy priv by order ref : " << e.order_ref;
					return;
				}

				break;
			}
			case order_event_type::rejected:
			{
				LOGGER << "order insert rejected";
				boost::unordered_map<int, strategy_priv*>::iterator i = orderref_2_priv_map_.find(e.order_ref);
				if(i == orderref_2_priv_map_.end())
				{
					LOGGER << "Failed to find strategy priv by order ref : " << e.order_ref;
					return;
				}

				if(i->second->far_pos.inst == e.inst)
				{
					LOGGER << "Insert request for far instrument is rejected, the combo is failed...";
					//TODO: Should return the failure to GUI
					return;
				}
				else if(i->second->near_pos.inst == e.inst)
				{
					LOGGER << "Insert request for near instrument is rejected, the far instrument needs to be closed...";
					if(i->second->reject_times == max_reject_times_)
					{
						//failed for max times when try near inst order, close the combo
						i->second->status = position_status::pending_close;
						close_position(i->second->far_pos, i->second->far_pos.open_trade_quantity);
					}
					//TODO close far instrument immediately?
				}

				break;
			}
			case order_event_type::fill:
			{
				LOGGER << "order filled";
				boost::unordered_map<int, strategy_priv*>::iterator i = orderref_2_priv_map_.find(e.order_ref);
				if(i == orderref_2_priv_map_.end())
				{
					LOGGER << "Failed to find strategy priv by order ref : " << e.order_ref;
					return;
				}

				if(e.inst == i->second->far_pos.inst)
				{
					//a trade for far inst come
					//if it is a open position than need to check price and send request to open near inst
					//if it is a close position not much need to do. It probably means the combo is during close

					position& pos = i->second->far_pos;

					if(e.offset == offset_flag::open)
					{
						pos.status = position_status::open;
						pos.open_trade_price = 
							((pos.open_trade_price * pos.open_trade_quantity + e.price * e.quantity)/(pos.open_trade_quantity + e.quantity));
						pos.open_trade_quantity += e.quantity;

						double last_px = pos.inst->tob.last_price;
						pos.open_balance = (pos.direction == position_direction::buy ? 
										(last_px - pos.open_trade_price)*pos.open_trade_quantity 
												: (pos.open_trade_price - last_px)*pos.open_trade_quantity);

						
							
						if(i->second->status == position_status::pending_close)
						{
							//the combo is during close no need to open any new position just close this position
							close_position(pos, e.quantity);
						}

						//far inst is opened, going to send insert request for near inst
						double near_last_px = i->second->near_pos.inst->tob.last_price;
						double far_last_px = i->second->far_pos.inst->tob.last_price;
						//if far inst is fully filled then need to compare the price margin 
						//and if there is still a opportinity then open near inst					
						if(pos.quantity == pos.open_trade_quantity)
						{
							double balance_percent = 
								pos.open_balance/pos.open_trade_quantity/pos.open_trade_price;
							if(balance_percent >= 0.02)
							{
								//if the win is not less than 2% just close the far and take the win
								//TODO 2% need to be configurable
								i->second->status = position_status::pending_close;
								close_position(i->second->far_pos,i->second->far_pos.open_trade_quantity);
							}
							//TODO the price margin check here needs to depend on a buy or sell margin
							else
							{
								double theo_balance_percent = 
									(near_last_px - far_last_px - i->second->price_margin) / i->second->price_margin;

								if(theo_balance_percent > 0 && (theo_balance_percent + balance_percent) > -0.02)
								{
									//and the loss is not greater than 2% and the current last price margin is still valid 
									int order_ref = 0;
									if(oe_.send_order_insert_request(i->second->near_pos.inst->id,  
										THOST_FTDC_OFEN_Open, THOST_FTDC_D_Sell, i->second->near_pos.quantity, order_ref))
									{
										i->second->near_pos.status = position_status::pending_open;
										i->second->near_pos.open_order_ref = order_ref;
										orderref_2_priv_map_[order_ref] = i->second;
										return;
									}
								}					
								
								close_position(i->second->far_pos,i->second->far_pos.open_trade_quantity);
								//TODO return this info to GUI
							}
						}
					}
					else
					{
						//a close trade need to update the balance
						pos.status = position_status::close;
						pos.close_trade_price = 
							((pos.close_trade_price * pos.close_trade_quantity + e.price * e.quantity)/(pos.close_trade_quantity + e.quantity));
						pos.close_trade_quantity += e.quantity;
						pos.close_balance = (pos.direction == position_direction::buy ? 
							(pos.close_trade_price - pos.open_trade_price ) * pos.close_trade_quantity 
									: (pos.open_trade_price - pos.close_trade_price) * pos.close_trade_quantity);
						

						//far inst is closed the whole combo is finished if near inst is closed or not even started
						if(pos.quantity == pos.close_trade_quantity && 
							i->second->near_pos.close_trade_quantity == i->second->near_pos.quantity 
							|| i->second->near_pos.status == position_status::initial)
						{
							on_combo_close(*(i->second));
						}
					}
				}
				else if(e.inst == i->second->near_pos.inst)
				{
					//a trade for near inst come
					//if it is an open it means both instruments of the combo is opened and the combo is waiting for close
					//if it is an close not much need to do it probably means the combo is during close
					position& pos = i->second->near_pos;
					if(e.offset == offset_flag::open)
					{
						pos.status = position_status::open;
						pos.open_trade_price = 
							((pos.open_trade_price * pos.open_trade_quantity + e.price * e.quantity)/(pos.open_trade_quantity + e.quantity));
						pos.open_trade_quantity += e.quantity;

						pos.open_balance = (pos.direction == position_direction::buy ? 
							(pos.inst->tob.last_price - pos.open_trade_price) * pos.open_trade_quantity 
										: (pos.open_trade_price - pos.inst->tob.last_price) * pos.open_trade_quantity);

						

						if(i->second->status == position_status::pending_close)
						{
							//the combo is during close no need to open any new position just close this position
							close_position(pos, e.quantity);
						}
						else if(i->second->status == position_status::pending_open)
						{
							i->second->status = position_status::open;
						}
					}
					else
					{
						//a close trade need to update the balance
						pos.status = position_status::close;
						pos.close_trade_price = 
							((pos.close_trade_price * pos.close_trade_quantity + e.price * e.quantity)/(pos.close_trade_quantity + e.quantity));
						pos.close_trade_quantity += e.quantity;
						pos.close_balance = (pos.direction == position_direction::buy ? 
							(pos.close_trade_price - pos.open_trade_price ) * pos.close_trade_quantity 
									: (pos.open_trade_price - pos.close_trade_price) * pos.close_trade_quantity);
						

						//far inst is closed the whole combo is finished if near inst is closed or not even started
						if(pos.quantity == pos.close_trade_quantity && 
							i->second->far_pos.close_trade_quantity == i->second->far_pos.quantity)
						{
							on_combo_close(*(i->second));
						}
					}
				}
				else
				{
					LOGGER << "unknow inst in trade event...";
				}

				break;
			}
			case order_event_type::cxl_accepted:
				{
					LOGGER << "order cancel acceptted";
					break;
				}
			case order_event_type::cxl_rejected:
				{
					LOGGER << "order cancel rejected";
					break;
				}
			default:
				{
					LOGGER << "unknown order event type";
					break;
				}
		}
	}

	void strategy::handle_md_update_event(md_event& e)
	{
		LOGGER << "instrument [" << e.internal_inst_id << "] tob - bid: " << e.bid_qty << "@" << e.bid_price
		<< " ask: " << e.ask_qty << "@" << e.ask_price << " last px: " << e.last_price;

		if(current_active_combo_ && current_active_combo_->status == position_status::open )
		{
			position& near_pos = current_active_combo_->near_pos;
			position& far_pos = current_active_combo_->far_pos;
			if(e.internal_inst_id == far_pos.inst->internal_id 
						&& far_pos.status == position_status::open)
			{
				double price_diff = 
					far_pos.direction == position_direction::buy ? 
						(e.last_price - far_pos.open_trade_price) : (far_pos.open_trade_price - e.last_price);
				far_pos.open_balance = price_diff * far_pos.open_trade_quantity;
			}
			else if (e.internal_inst_id == near_pos.inst->internal_id && near_pos.status == position_status::open)
			{
				double price_diff = near_pos.direction == position_direction::buy ? 
													(e.last_price - near_pos.open_trade_price)
															: (near_pos.open_trade_price - e.last_price);
				near_pos.open_balance = price_diff * near_pos.open_trade_quantity;
			}

			if(near_pos.open_trade_quantity == near_pos.quantity && far_pos.open_trade_quantity == far_pos.quantity)
			{
				double balance_percent = 
					far_pos.open_balance/far_pos.open_trade_quantity/far_pos.close_trade_price 
						+ near_pos.open_balance/near_pos.open_trade_quantity/near_pos.open_trade_price;
				if(balance_percent > 0.02 || balance_percent < -0.02)
				{
					current_active_combo_->status = position_status::pending_close;
					close_position(far_pos, far_pos.open_trade_quantity);
					close_position(near_pos, near_pos.open_trade_quantity);
				}
			}
		}
	}

	void strategy::close_position(position& pos, uint32_t qty)
	{
		char offset = (pos.direction == position_direction::buy ?  THOST_FTDC_DEN_Sell :  THOST_FTDC_DEN_Buy);
		oe_.send_order_insert_request(pos.inst->id, 
			THOST_FTDC_OF_Close, offset, qty, pos.close_order_ref);
		pos.status = position_status::pending_close;
	}

	void strategy::on_combo_close(strategy_priv& combo)
	{
		LOGGER    << "one combo is closed between " << combo.near_pos.inst->id << " - " << combo.far_pos.inst->id 
				  << " Near position [open: " << combo.near_pos.open_trade_quantity << "@" << combo.near_pos.open_trade_price
				  << " close: " << combo.near_pos.close_trade_quantity << "@" << combo.near_pos.close_trade_price 
				  << "] Far position [open: " << combo.far_pos.open_trade_quantity << "@" << combo.far_pos.open_trade_price
				  << " close: " << combo.far_pos.close_trade_quantity << "@" << combo.far_pos.close_trade_price 
				  << "] Total cache value balance: " << (combo.near_pos.close_balance + combo.far_pos.close_balance);
		
		//update combo status and reset the current active combo
		current_active_combo_->status = position_status::close;
		current_active_combo_ = 0;
	}
}
