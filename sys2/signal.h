/**
 * @file signal.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#pragma once
#include <mutex>
#include <condition_variable>

namespace sys {

class signal
{
private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_predicate = false;
	bool m_auto_reset = true;
public:
	signal(bool auto_reset=true);
	~signal();

	void set_auto_reset(bool auto_reset) { m_auto_reset = auto_reset; }
	void wait();
	bool wait(int msec);
	void notify();

	void reset() {
		m_predicate = false;
	}

	bool has_signal()const { return m_predicate; }
private:
	bool predicate() {
		return m_predicate;
	}

};

}
