#pragma once
#include <opal/call.h>
#include <map>
#include <string>
#include <future>
#include <shared_mutex>
#include <sys2/signal.h>


namespace voip
{
	class calling_map
	{
		struct calling_item
		{
			PSafePtr<OpalCall> call;
			time_t time = 0;
		};
	public:
		calling_map();
		~calling_map();

		bool start();;
		void stop();

		bool add(const std::string& id, PSafePtr<OpalCall> call);
		PSafePtr<OpalCall> pop(const std::string& id);
		size_t count();
		std::vector<PSafePtr<OpalCall>> all();
		void clear_expired();
		void clear();
	private:
		void on_timer();
	private:
		bool active_ = false;
		sys::signal signal_;
		std::future<void> timer_;
		std::shared_mutex mutex_;
		std::map<std::string, calling_item> map_;
		int expired_ = 10;
	};


}