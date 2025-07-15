#pragma once
#include <map>
#include <string>
#include <mutex>
#include "signal.h"

namespace sys
{
	template<class TReq,class TRsp>
	class echo
	{
		struct item_t
		{
			std::string id;
			TReq req;
			TRsp rsp;
			int64_t ts=0;
			sys::signal sig;
			bool completed = false;
		};
	public:
		bool set(const std::string& id, const TReq& req)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			auto itr=items_.find(id);
			if (itr != items_.end())
			{
				return false;
			}

			auto item=std::make_shared<item_t>();
			item->id = id;
			item->req = req;
			item->ts = time(nullptr);
			item->completed = false;
			auto r=items_.emplace(id, item);
			return r.second;
		}

		bool response(const std::string& id, const TRsp& rsp)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			auto itr = items_.find(id);
			if (itr == items_.end())
				return false;

			itr->second->rsp = rsp;
			itr->second->completed = true;
			itr->second->sig.notify();
			return true;
		}

		bool get(const std::string& id, TRsp& rsp, int msec)
		{
			std::shared_ptr<item_t> item;
			{
				std::lock_guard<std::recursive_mutex> lk(mutex_);
				auto itr = items_.find(id);
				if (itr == items_.end())
				{
					return false;
				}
				item = itr->second;
			}
			if (!item->sig.wait(msec))
			{
				std::lock_guard<std::recursive_mutex> lk(mutex_);
				items_.erase(id);
				return false;
			}
			{
				std::lock_guard<std::recursive_mutex> lk(mutex_);
				items_.erase(id);
				rsp = item->rsp;
			}
			return true;
		}

		bool try_get(const std::string& id,TReq& req, TRsp& rsp, int msec)
		{
			std::shared_ptr<item_t> item;
			{
				std::lock_guard<std::recursive_mutex> lk(mutex_);
				auto itr = items_.find(id);
				if (itr == items_.end())
				{
					return false;
				}
				item = itr->second;
			}
			if (!item->sig.wait(msec))
			{
				req = item->req;
				return false;
			}
			req = item->req;
			{
				std::lock_guard<std::recursive_mutex> lk(mutex_);
				items_.erase(id);
				rsp = item->rsp;
			}
			return true;
		}

		void remove(const std::string& id)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			items_.erase(id);
		}


	private:
		std::recursive_mutex mutex_;
		std::map<std::string, std::shared_ptr<item_t>> items_;
	};
}